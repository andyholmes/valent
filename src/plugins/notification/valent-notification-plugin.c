// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-notification-plugin"

#include "config.h"

#include <gtk/gtk.h>
#include <libportal/portal.h>
#include <gio/gio.h>
#include <json-glib/json-glib.h>
#include <valent.h>

#include "valent-notification-dialog.h"
#include "valent-notification-plugin.h"
#include "valent-notification-upload.h"

#define DEFAULT_ICON_SIZE 512


struct _ValentNotificationPlugin
{
  ValentDevicePlugin   parent_instance;

  GCancellable        *cancellable;

  ValentNotifications *notifications;
  ValentSession       *session;
  GHashTable          *dialogs;
};

G_DEFINE_FINAL_TYPE (ValentNotificationPlugin, valent_notification_plugin, VALENT_TYPE_DEVICE_PLUGIN)

static void valent_notification_plugin_handle_notification         (ValentNotificationPlugin *self,
                                                                    JsonNode                 *packet);
static void valent_notification_plugin_handle_notification_action  (ValentNotificationPlugin *self,
                                                                    JsonNode                 *packet);
static void valent_notification_plugin_handle_notification_reply   (ValentNotificationPlugin *self,
                                                                    JsonNode                 *packet);
static void valent_notification_plugin_handle_notification_request (ValentNotificationPlugin *self,
                                                                    JsonNode                 *packet);

static void valent_notification_plugin_close_notification          (ValentNotificationPlugin *self,
                                                                    const char               *id);
static void valent_notification_plugin_request_notifications       (ValentNotificationPlugin *self);
static void valent_notification_plugin_send_notification           (ValentNotificationPlugin *self,
                                                                    const char               *id,
                                                                    const char               *appName,
                                                                    const char               *title,
                                                                    const char               *body,
                                                                    GIcon                    *icon);
static void valent_notification_plugin_show_notification           (ValentNotificationPlugin *self,
                                                                    JsonNode                 *packet,
                                                                    GIcon                    *gicon);


/*
 * ValentNotifications Callbacks
 */
static void
on_notification_added (ValentNotifications      *listener,
                       ValentNotification       *notification,
                       ValentNotificationPlugin *self)
{
  GSettings *settings;
  ValentDevice *device;
  const char *application;
  g_auto (GStrv) deny = NULL;

  g_assert (VALENT_IS_NOTIFICATIONS (listener));
  g_assert (VALENT_IS_NOTIFICATION (notification));
  g_assert (VALENT_IS_NOTIFICATION_PLUGIN (self));

  settings = valent_device_plugin_get_settings (VALENT_DEVICE_PLUGIN (self));

  if (!g_settings_get_boolean (settings, "forward-notifications"))
    return;

  if (!g_settings_get_boolean (settings, "forward-when-active") &&
      valent_session_get_active (self->session))
    return;

  application = valent_notification_get_application (notification);
  deny = g_settings_get_strv (settings, "forward-deny");

  if (application && g_strv_contains ((const char * const *)deny, application))
    return;

  device = valent_device_plugin_get_device (VALENT_DEVICE_PLUGIN (self));

  if ((valent_device_get_state (device) & VALENT_DEVICE_STATE_CONNECTED) == 0)
    {
      VALENT_TODO ("Cache notifications for later sending?");
      return;
    }

  valent_notification_plugin_send_notification (self,
                                                valent_notification_get_id (notification),
                                                valent_notification_get_application (notification),
                                                valent_notification_get_title (notification),
                                                valent_notification_get_body (notification),
                                                valent_notification_get_icon (notification));
}

static void
on_notification_removed (ValentNotifications      *listener,
                         const char               *id,
                         ValentNotificationPlugin *self)
{
  ValentDevice *device;

  g_assert (VALENT_IS_NOTIFICATIONS (listener));
  g_assert (id != NULL);

  device = valent_device_plugin_get_device (VALENT_DEVICE_PLUGIN (self));

  if ((valent_device_get_state (device) & VALENT_DEVICE_STATE_CONNECTED) == 0)
    {
      VALENT_TODO ("Cache notifications for later removal?");
      return;
    }

  valent_notification_plugin_close_notification (self, id);
}

/*
 * Icon Transfers
 */
static GFile *
valent_notification_plugin_get_icon_file (ValentNotificationPlugin *self,
                                          JsonNode                 *packet)
{
  g_autoptr (GFile) file = NULL;
  const char *payload_hash;

  g_assert (VALENT_IS_NOTIFICATION_PLUGIN (self));
  g_assert (VALENT_IS_PACKET (packet));

  if (valent_packet_get_string (packet, "payloadHash", &payload_hash))
    {
      ValentContext *context = NULL;

      context = valent_device_plugin_get_context (VALENT_DEVICE_PLUGIN (self));
      file = valent_context_get_cache_file (context, payload_hash);
    }
  else
    {
      g_autoptr (GFileIOStream) stream = NULL;

      file = g_file_new_tmp ("valent-notification-icon.XXXXXX", &stream, NULL);
    }

  return g_steal_pointer (&file);
}

static void
download_icon_task (GTask        *task,
                    gpointer      source_object,
                    gpointer      task_data,
                    GCancellable *cancellable)
{
  ValentNotificationPlugin *self = VALENT_NOTIFICATION_PLUGIN (source_object);
  JsonNode *packet = task_data;
  g_autoptr (GFile) file = NULL;
  GError *error = NULL;

  g_assert (VALENT_IS_NOTIFICATION_PLUGIN (self));
  g_assert (VALENT_IS_PACKET (packet));

  if (g_task_return_error_if_cancelled (task))
    return;

  file = valent_notification_plugin_get_icon_file (self, packet);

  /* Check if we've already downloaded this icon */
  if (!g_file_query_exists (file, cancellable))
    {
      ValentDevice *device;
      g_autoptr (GIOStream) source = NULL;
      g_autoptr (GFileOutputStream) target = NULL;
      g_autoptr (GFile) cache_dir = NULL;
      g_autoptr (ValentChannel) channel = NULL;

      /* Ensure the cache directory exists */
      cache_dir = g_file_get_parent (file);

      if (g_mkdir_with_parents (g_file_peek_path (cache_dir), 0700) != 0)
        {
          return g_task_return_new_error (task,
                                          G_IO_ERROR,
                                          G_IO_ERROR_FAILED,
                                          "Error: %s",
                                          g_strerror (errno));
        }

      /* Get the device channel */
      device = valent_device_plugin_get_device (VALENT_DEVICE_PLUGIN (self));

      if ((channel = valent_device_ref_channel (device)) == NULL)
        {
          return g_task_return_new_error (task,
                                          G_IO_ERROR,
                                          G_IO_ERROR_NOT_CONNECTED,
                                          "Device is disconnected");
        }

      source = valent_channel_download (channel, packet, cancellable, &error);

      if (source == NULL)
        {
          g_file_delete (file, NULL, NULL);
          return g_task_return_error (task, error);
        }

      /* Get the output stream */
      target = g_file_replace (file,
                               NULL,
                               FALSE,
                               G_FILE_CREATE_REPLACE_DESTINATION,
                               cancellable,
                               &error);

      if (target == NULL)
        {
          g_file_delete (file, NULL, NULL);
          return g_task_return_error (task, error);
        }

      /* Start download */
      g_output_stream_splice (G_OUTPUT_STREAM (target),
                              g_io_stream_get_input_stream (source),
                              (G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE |
                               G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET),
                              cancellable,
                              &error);

      if (error != NULL)
        {
          g_file_delete (file, NULL, NULL);
          return g_task_return_error (task, error);
        }
    }

  /* If we're in a sandbox, send the file as a GBytesIcon in case the file path
   * is not valid for the host system. */
  if (xdp_portal_running_under_sandbox ())
    {
      g_autoptr (GBytes) bytes = NULL;

      bytes = g_file_load_bytes (file, cancellable, NULL, &error);

      if (bytes == NULL)
        {
          g_file_delete (file, NULL, NULL);
          return g_task_return_error (task, error);
        }

      g_task_return_pointer (task, g_bytes_icon_new (bytes), g_object_unref);
    }
  else
    {
      g_task_return_pointer (task, g_file_icon_new (file), g_object_unref);
    }
}

static void
valent_notification_plugin_download_icon (ValentNotificationPlugin *self,
                                          JsonNode                 *packet,
                                          GCancellable             *cancellable,
                                          GAsyncReadyCallback       callback,
                                          gpointer                  user_data)
{
  g_autoptr (GTask) task = NULL;

  g_assert (VALENT_IS_NOTIFICATION_PLUGIN (self));
  g_assert (VALENT_IS_PACKET (packet));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_notification_plugin_download_icon);
  g_task_set_task_data (task,
                        json_node_ref (packet),
                        (GDestroyNotify)json_node_unref);
  g_task_run_in_thread (task, download_icon_task);
}

static GIcon *
valent_notification_plugin_download_icon_finish (ValentNotificationPlugin  *self,
                                                 GAsyncResult              *result,
                                                 GError                   **error)
{
  g_assert (VALENT_IS_NOTIFICATION_PLUGIN (self));
  g_assert (g_task_is_valid (result, self));
  g_assert (error == NULL || *error == NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}

/*
 * Remote Notifications
 */
static void
valent_notification_plugin_show_notification (ValentNotificationPlugin *self,
                                              JsonNode                 *packet,
                                              GIcon                    *gicon)
{
  ValentDevice *device;
  g_autoptr (GNotification) notification = NULL;
  g_autoptr (GIcon) icon = NULL;
  g_auto (GStrv) ticker_strv = NULL;
  const char *id;
  const char *app_name = NULL;
  const char *title = NULL;
  const char *text = NULL;
  const char *ticker;
  const char *reply_id;
  JsonArray *actions;

  /* Finish the icon task */
  if (G_IS_ICON (gicon))
    icon = g_object_ref (gicon);

  /* Ensure we have a notification id */
  if (!valent_packet_get_string (packet, "id", &id))
    {
      g_debug ("%s(): expected \"id\" field holding a string",
               G_STRFUNC);
      return;
    }

  /* This should never be absent, but we check anyways */
  if (!valent_packet_get_string (packet, "appName", &app_name))
    {
      g_debug ("%s(): expected \"appName\" field holding a string",
               G_STRFUNC);
      return;
    }

  /* If `title` or `text` are missing, try to compose them from `ticker` */
  if ((!valent_packet_get_string (packet, "title", &title) ||
       !valent_packet_get_string (packet, "text", &text)) &&
      valent_packet_get_string (packet, "ticker", &ticker))
    {
      ticker_strv = g_strsplit (ticker, ": ", 2);
      title = ticker_strv[0];
      text = ticker_strv[1];
    }

  if (title == NULL || text == NULL)
    {
      g_debug ("%s(): expected either title and text, or ticker",
               G_STRFUNC);
      return;
    }

  device = valent_device_plugin_get_device (VALENT_DEVICE_PLUGIN (self));

  /* Start building the GNotification */
  notification = g_notification_new (title);

  /* Repliable Notification */
  if (valent_packet_get_string (packet, "requestReplyId", &reply_id))
    {
      g_autoptr (ValentNotification) incoming = NULL;
      const char *time_str = NULL;
      gint64 time = 0;
      GVariant *target;

      if (valent_packet_get_string (packet, "time", &time_str))
        time = g_ascii_strtoll (time_str, NULL, 10);

      incoming = g_object_new (VALENT_TYPE_NOTIFICATION,
                               "id",          id,
                               "application", app_name,
                               "icon",        icon,
                               "title",       title,
                               "body",        text,
                               "time",        time,
                               NULL);
      target = g_variant_new ("(ssv)",
                              reply_id,
                              "",
                              valent_notification_serialize (incoming));

      valent_notification_set_device_action (notification,
                                             device,
                                             "notification.reply",
                                             target);
    }

  /* Notification Actions */
  if (valent_packet_get_array (packet, "actions", &actions))
    {
      unsigned int n_actions;

      n_actions = json_array_get_length (actions);

      for (unsigned int i = 0; i < n_actions; i++)
        {
          JsonNode *element;
          const char *action;
          GVariant *target;

          if ((element = json_array_get_element (actions, i)) == NULL ||
              json_node_get_value_type (element) != G_TYPE_STRING)
            continue;

          action = json_node_get_string (element);
          target = g_variant_new ("(ss)", id, action);
          valent_notification_add_device_button (notification,
                                                 device,
                                                 action,
                                                 "notification.action",
                                                 target);
        }
    }

  /* Special cases.
   *
   * In some cases, usually on Android, a notification "type" can be inferred
   * from the ID string or the appName is duplicated as the title.
   */
  if (g_strrstr (id, "MissedCall") != NULL)
    {
      g_notification_set_title (notification, title);
      g_notification_set_body (notification, text);

      if (icon == NULL)
        icon = g_themed_icon_new ("call-missed-symbolic");
    }
  else if (g_strrstr (id, "sms") != NULL)
    {
      g_notification_set_title (notification, title);
      g_notification_set_body (notification, text);

      if (icon == NULL)
        icon = g_themed_icon_new ("sms-symbolic");
    }
  else if (g_strcmp0 (app_name, title) == 0)
    {
      g_notification_set_title (notification, title);
      g_notification_set_body (notification, text);
    }
  else
    {
      g_autofree char *ticker_body = NULL;

      ticker_body = g_strdup_printf ("%s: %s", title, text);
      g_notification_set_title (notification, app_name);
      g_notification_set_body (notification, ticker_body);
    }

  if (icon != NULL)
    g_notification_set_icon (notification, icon);

  valent_device_plugin_show_notification (VALENT_DEVICE_PLUGIN (self),
                                          id,
                                          notification);
}

static void
valent_notification_plugin_download_icon_cb (ValentNotificationPlugin *self,
                                             GAsyncResult             *result,
                                             gpointer                  user_data)
{
  g_autoptr (JsonNode) packet = user_data;
  g_autoptr (GIcon) icon = NULL;
  g_autoptr (GError) error = NULL;

  g_assert (VALENT_IS_NOTIFICATION_PLUGIN (self));
  g_assert (g_task_is_valid (result, self));

  icon = valent_notification_plugin_download_icon_finish (self, result, &error);

  if (icon == NULL)
    {
      /* If the operation was cancelled, the plugin is being disposed */
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        return;

      g_warning ("Downloading icon: %s", error->message);
    }

  valent_notification_plugin_show_notification (self, packet, icon);
}

static void
valent_notification_plugin_handle_notification (ValentNotificationPlugin *self,
                                                JsonNode                 *packet)
{
  g_assert (VALENT_IS_NOTIFICATION_PLUGIN (self));
  g_assert (VALENT_IS_PACKET (packet));

  /* A report that a remote notification has been dismissed */
  if (valent_packet_check_field (packet, "isCancel"))
    {
      const char *id;

      if (!valent_packet_get_string (packet, "id", &id))
        {
          g_debug ("%s(): expected \"id\" field holding a string",
                   G_STRFUNC);
          return;
        }

      valent_device_plugin_hide_notification (VALENT_DEVICE_PLUGIN (self), id);
      return;
    }

  /* A notification that should only be shown once */
  if (valent_packet_check_field (packet, "onlyOnce"))
    VALENT_TODO ("Handle 'onlyOnce' field");

  /* A notification with an icon payload */
  if (valent_packet_has_payload (packet))
    {
      valent_notification_plugin_download_icon (self,
                                                packet,
                                                self->cancellable,
                                                (GAsyncReadyCallback)valent_notification_plugin_download_icon_cb,
                                                json_node_ref (packet));
    }

  /* A notification without an icon payload */
  else
    {
      valent_notification_plugin_show_notification (self, packet, NULL);
    }
}

static void
valent_notification_plugin_handle_notification_action (ValentNotificationPlugin *self,
                                                       JsonNode                 *packet)
{
  g_assert (VALENT_IS_NOTIFICATION_PLUGIN (self));
  g_assert (VALENT_IS_PACKET (packet));

  VALENT_TODO ("kdeconnect.notification.action");
}

static void
valent_notification_plugin_handle_notification_reply (ValentNotificationPlugin *self,
                                                      JsonNode                 *packet)
{
  g_assert (VALENT_IS_NOTIFICATION_PLUGIN (self));
  g_assert (VALENT_IS_PACKET (packet));

  VALENT_TODO ("kdeconnect.notification.reply");
}

static void
valent_notification_plugin_handle_notification_request (ValentNotificationPlugin *self,
                                                        JsonNode                 *packet)
{
  g_assert (VALENT_IS_NOTIFICATION_PLUGIN (self));
  g_assert (VALENT_IS_PACKET (packet));

  VALENT_TODO ("kdeconnect.notification.request");
}

static void
valent_notification_plugin_request_notifications (ValentNotificationPlugin *self)
{
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonNode) packet = NULL;

  g_assert (VALENT_IS_NOTIFICATION_PLUGIN (self));

  valent_packet_init (&builder, "kdeconnect.notification.request");
  json_builder_set_member_name (builder, "request");
  json_builder_add_boolean_value (builder, TRUE);
  packet = valent_packet_end (&builder);

  valent_device_plugin_queue_packet (VALENT_DEVICE_PLUGIN (self), packet);
}

static void
valent_notification_plugin_close_notification (ValentNotificationPlugin *self,
                                               const char               *id)
{
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonNode) packet = NULL;

  g_assert (VALENT_IS_NOTIFICATION_PLUGIN (self));
  g_assert (id != NULL);

  valent_packet_init (&builder, "kdeconnect.notification.request");
  json_builder_set_member_name (builder, "cancel");
  json_builder_add_string_value (builder, id);
  packet = valent_packet_end (&builder);

  valent_device_plugin_queue_packet (VALENT_DEVICE_PLUGIN (self), packet);
}

static void
valent_notification_upload_execute_cb (GObject      *object,
                                       GAsyncResult *result,
                                       gpointer      user_data)
{
  ValentTransfer *transfer = VALENT_TRANSFER (object);
  g_autoptr (GError) error = NULL;

  if (!valent_transfer_execute_finish (transfer, result, &error) &&
      !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
      g_autoptr (ValentDevice) device = NULL;
      g_autoptr (JsonNode) packet = NULL;

      g_object_get (transfer,
                    "device", &device,
                    "packet", &packet,
                    NULL);
      valent_device_queue_packet (device, packet);
    }
}

static void
valent_notification_plugin_send_notification_with_icon (ValentNotificationPlugin *self,
                                                        JsonNode                 *packet,
                                                        GIcon                    *icon)
{
  g_assert (VALENT_IS_NOTIFICATION_PLUGIN (self));
  g_assert (VALENT_IS_PACKET (packet));
  g_assert (icon == NULL || G_IS_ICON (icon));

  if (G_IS_ICON (icon))
    {
      ValentDevice *device;
      g_autoptr (ValentTransfer) transfer = NULL;

      device = valent_device_plugin_get_device (VALENT_DEVICE_PLUGIN (self));
      transfer = valent_notification_upload_new (device, packet, icon);
      valent_transfer_execute (transfer,
                               NULL,
                               valent_notification_upload_execute_cb,
                               NULL);
      return;
    }

  valent_device_plugin_queue_packet (VALENT_DEVICE_PLUGIN (self), packet);
}

/**
 * valent_notification_plugin_send_notification:
 * @self: a #ValentNotificationPlugin
 * @id: the notification id
 * @appName: (nullable): the notifying application
 * @title: (nullable): the notification title
 * @body: (nullable): the notification body
 * @icon: (nullable): a #GIcon
 *
 * Send a notification to the remote device.
 */
static void
valent_notification_plugin_send_notification (ValentNotificationPlugin *self,
                                              const char               *id,
                                              const char               *appName,
                                              const char               *title,
                                              const char               *body,
                                              GIcon                    *icon)
{
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonNode) packet = NULL;
  g_autofree char *ticker = NULL;

  g_return_if_fail (VALENT_IS_NOTIFICATION_PLUGIN (self));
  g_return_if_fail (id != NULL);
  g_return_if_fail (icon == NULL || G_IS_ICON (icon));

  /* Build packet */
  valent_packet_init (&builder, "kdeconnect.notification");
  json_builder_set_member_name (builder, "id");
  json_builder_add_string_value (builder, id);

  /* Application Name */
  json_builder_set_member_name (builder, "appName");
  json_builder_add_string_value (builder, appName ? appName : "Valent");

  /* Title & Body (aka Ticker) */
  json_builder_set_member_name (builder, "title");
  json_builder_add_string_value (builder, title ? title : "");
  json_builder_set_member_name (builder, "body");
  json_builder_add_string_value (builder, body ? body : "");

  ticker = g_strdup_printf ("%s: %s", title, body);
  json_builder_set_member_name (builder, "ticker");
  json_builder_add_string_value (builder, ticker);

  packet = valent_packet_end (&builder);

  if (icon == NULL)
    valent_device_plugin_queue_packet (VALENT_DEVICE_PLUGIN (self), packet);
  else
    valent_notification_plugin_send_notification_with_icon (self, packet, icon);
}

/*
 * GActions
 */
static void
notification_action_action (GSimpleAction *action,
                            GVariant      *parameter,
                            gpointer       user_data)
{
  ValentNotificationPlugin *self = VALENT_NOTIFICATION_PLUGIN (user_data);
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonNode) packet = NULL;
  char *id;
  char *name;

  g_assert (VALENT_IS_NOTIFICATION_PLUGIN (self));

  g_variant_get (parameter, "(&s&s)", &id, &name);

  valent_packet_init (&builder, "kdeconnect.notification.action");
  json_builder_set_member_name (builder, "key");
  json_builder_add_string_value (builder, id);
  json_builder_set_member_name (builder, "action");
  json_builder_add_string_value (builder, name);
  packet = valent_packet_end (&builder);

  valent_device_plugin_queue_packet (VALENT_DEVICE_PLUGIN (self), packet);
}

static void
notification_cancel_action (GSimpleAction *action,
                            GVariant      *parameter,
                            gpointer       user_data)
{
  ValentNotificationPlugin *self = VALENT_NOTIFICATION_PLUGIN (user_data);
  const char *id;
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonNode) packet = NULL;

  g_assert (VALENT_IS_NOTIFICATION_PLUGIN (self));

  id = g_variant_get_string (parameter, NULL);

  valent_packet_init (&builder, "kdeconnect.notification");
  json_builder_set_member_name (builder, "id");
  json_builder_add_string_value (builder, id);
  json_builder_set_member_name (builder, "isCancel");
  json_builder_add_boolean_value (builder, TRUE);
  packet = valent_packet_end (&builder);

  valent_device_plugin_queue_packet (VALENT_DEVICE_PLUGIN (self), packet);
}

static void
notification_close_action (GSimpleAction *action,
                           GVariant      *parameter,
                           gpointer       user_data)
{
  ValentNotificationPlugin *self = VALENT_NOTIFICATION_PLUGIN (user_data);
  const char *id;

  g_assert (VALENT_IS_NOTIFICATION_PLUGIN (self));

  id = g_variant_get_string (parameter, NULL);
  valent_notification_plugin_close_notification (self, id);
}

static void
on_notification_reply (ValentNotificationDialog *dialog,
                       int                       response_id,
                       ValentNotificationPlugin *self)
{
  ValentNotification *notification = NULL;
  g_autofree char *reply = NULL;
  const char *reply_id;

  g_assert (VALENT_IS_NOTIFICATION_DIALOG (dialog));
  g_assert (VALENT_IS_NOTIFICATION_PLUGIN (self));

  notification = valent_notification_dialog_get_notification (dialog);
  reply = valent_notification_dialog_get_reply (dialog);
  reply_id = valent_notification_dialog_get_reply_id (dialog);

  if (response_id == GTK_RESPONSE_OK && *reply != '\0')
    {
      g_autoptr (JsonBuilder) builder = NULL;
      g_autoptr (JsonNode) packet = NULL;

      valent_packet_init (&builder, "kdeconnect.notification.reply");
      json_builder_set_member_name (builder, "requestReplyId");
      json_builder_add_string_value (builder, reply_id);
      json_builder_set_member_name (builder, "message");
      json_builder_add_string_value (builder, reply);
      packet = valent_packet_end (&builder);

      valent_device_plugin_queue_packet (VALENT_DEVICE_PLUGIN (self), packet);
    }

  g_hash_table_remove (self->dialogs, notification);
}

static void
notification_reply_action (GSimpleAction *action,
                           GVariant      *parameter,
                           gpointer       user_data)
{
  ValentNotificationPlugin *self = VALENT_NOTIFICATION_PLUGIN (user_data);
  const char *reply_id;
  const char *message;
  g_autoptr (GVariant) notificationv = NULL;

  g_assert (VALENT_IS_NOTIFICATION_PLUGIN (self));

  g_variant_get (parameter, "(&s&sv)", &reply_id, &message, &notificationv);

  /* If the reply ID is empty, we've received a broken request */
  if (reply_id == NULL || *reply_id == '\0')
    {
      g_debug ("%s(): expected requestReplyId", G_STRFUNC);
      return;
    }

  /* If the message is empty, we're being asked to show the dialog */
  if (message == NULL || *message == '\0')
    {
      g_autoptr (ValentNotificationDialog) dialog = NULL;
      g_autoptr (ValentNotification) notification = NULL;

      if (!gtk_is_initialized ())
        {
          g_warning ("%s: No display available", G_STRFUNC);
          return;
        }

      notification = valent_notification_deserialize (notificationv);

      if ((dialog = g_hash_table_lookup (self->dialogs, notification)) == NULL)
        {
          ValentDevice *device;
          ValentDeviceState state;
          gboolean available;

          dialog = g_object_new (VALENT_TYPE_NOTIFICATION_DIALOG,
                                 "notification",   notification,
                                 "reply-id",       reply_id,
                                 "use-header-bar", TRUE,
                                 NULL);
          g_signal_connect (dialog,
                            "response",
                            G_CALLBACK (on_notification_reply),
                            self);
          g_hash_table_insert (self->dialogs,
                               g_object_ref (notification),
                               g_object_ref (dialog));

          device = valent_device_plugin_get_device (VALENT_DEVICE_PLUGIN (self));
          state = valent_device_get_state (device);
          available = (state & VALENT_DEVICE_STATE_CONNECTED) != 0 &&
                      (state & VALENT_DEVICE_STATE_PAIRED) != 0;
          valent_notification_dialog_update_state (dialog, available);
        }

      gtk_window_present (GTK_WINDOW (dialog));
    }
  else
    {
      g_autoptr (JsonBuilder) builder = NULL;
      g_autoptr (JsonNode) packet = NULL;

      valent_packet_init (&builder, "kdeconnect.notification.reply");
      json_builder_set_member_name (builder, "requestReplyId");
      json_builder_add_string_value (builder, reply_id);
      json_builder_set_member_name (builder, "message");
      json_builder_add_string_value (builder, message);
      packet = valent_packet_end (&builder);

      valent_device_plugin_queue_packet (VALENT_DEVICE_PLUGIN (self), packet);
    }
}

static void
notification_send_action (GSimpleAction *action,
                          GVariant      *parameter,
                          gpointer       user_data)
{
  ValentNotificationPlugin *self = VALENT_NOTIFICATION_PLUGIN (user_data);
  GVariantDict dict;
  g_autofree char *id = NULL;
  const char *app = NULL;
  const char *title = NULL;
  const char *body = NULL;
  g_autoptr (GVariant) iconv = NULL;
  g_autoptr (GIcon) icon = NULL;

  g_assert (VALENT_IS_NOTIFICATION_PLUGIN (self));

  g_variant_dict_init (&dict, parameter);

  /* Use a random ID as a fallback */
  if (!g_variant_dict_lookup (&dict, "id", "s", &id))
    id = g_uuid_string_random ();

  g_variant_dict_lookup (&dict, "application", "&s", &app);
  g_variant_dict_lookup (&dict, "title", "&s", &title);
  g_variant_dict_lookup (&dict, "body", "&s", &body);

  /* Check for a serialized GIcon */
  iconv = g_variant_dict_lookup_value (&dict, "icon", G_VARIANT_TYPE ("(sv)"));

  if (iconv != NULL)
    icon = g_icon_deserialize (iconv);

  valent_notification_plugin_send_notification (self, id, app, title, body, icon);

  g_variant_dict_clear (&dict);
}

static const GActionEntry actions[] = {
    {"action", notification_action_action, "(ss)",  NULL, NULL},
    {"cancel", notification_cancel_action, "s",     NULL, NULL},
    {"close",  notification_close_action,  "s",     NULL, NULL},
    {"reply",  notification_reply_action,  "(ssv)", NULL, NULL},
    {"send",   notification_send_action,   "a{sv}", NULL, NULL},
};

/*
 * ValentDevicePlugin
 */
static void
valent_notification_plugin_enable (ValentDevicePlugin *plugin)
{
  ValentNotificationPlugin *self = VALENT_NOTIFICATION_PLUGIN (plugin);

  g_assert (VALENT_IS_NOTIFICATION_PLUGIN (self));

  self->cancellable = g_cancellable_new ();

  g_action_map_add_action_entries (G_ACTION_MAP (plugin),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   plugin);

  /* Watch for new local notifications */
  self->session = valent_session_get_default ();
  self->notifications = valent_notifications_get_default();
  g_signal_connect (self->notifications,
                    "notification-added",
                    G_CALLBACK (on_notification_added),
                    self);
  g_signal_connect (self->notifications,
                    "notification-removed",
                    G_CALLBACK (on_notification_removed),
                    self);

  self->dialogs = g_hash_table_new_full (valent_notification_hash,
                                         valent_notification_equal,
                                         g_object_unref,
                                         (GDestroyNotify)gtk_window_destroy);
}

static void
valent_notification_plugin_disable (ValentDevicePlugin *plugin)
{
  ValentNotificationPlugin *self = VALENT_NOTIFICATION_PLUGIN (plugin);

  /* Close any open reply dialogs */
  g_clear_pointer (&self->dialogs, g_hash_table_unref);

  /* Stop watching for local notifications and cancel pending transfers */
  if (self->notifications)
    g_signal_handlers_disconnect_by_data (self->notifications, self);

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);
}

static void
valent_notification_plugin_update_state (ValentDevicePlugin *plugin,
                                         ValentDeviceState   state)
{
  ValentNotificationPlugin *self = VALENT_NOTIFICATION_PLUGIN (plugin);
  GHashTableIter iter;
  ValentNotificationDialog *dialog;
  gboolean available;

  g_assert (VALENT_IS_NOTIFICATION_PLUGIN (self));

  available = (state & VALENT_DEVICE_STATE_CONNECTED) != 0 &&
              (state & VALENT_DEVICE_STATE_PAIRED) != 0;

  valent_device_plugin_toggle_actions (plugin, available);

  /* Request Notifications */
  if (available)
    valent_notification_plugin_request_notifications (self);

  /* Update Reply Dialogs */
  g_hash_table_iter_init (&iter, self->dialogs);

  while (g_hash_table_iter_next (&iter, NULL, (void **)&dialog))
    valent_notification_dialog_update_state (dialog, available);

  /* TODO: send active notifications */
}

static void
valent_notification_plugin_handle_packet (ValentDevicePlugin *plugin,
                                          const char         *type,
                                          JsonNode           *packet)
{
  ValentNotificationPlugin *self = VALENT_NOTIFICATION_PLUGIN (plugin);

  g_assert (VALENT_IS_NOTIFICATION_PLUGIN (plugin));
  g_assert (type != NULL);
  g_assert (VALENT_IS_PACKET (packet));

  if (strcmp (type, "kdeconnect.notification") == 0)
    valent_notification_plugin_handle_notification (self, packet);

  else if (strcmp (type, "kdeconnect.notification.action") == 0)
    valent_notification_plugin_handle_notification_action (self, packet);

  else if (strcmp (type, "kdeconnect.notification.reply") == 0)
    valent_notification_plugin_handle_notification_reply (self, packet);

  else if (strcmp (type, "kdeconnect.notification.request") == 0)
    valent_notification_plugin_handle_notification_request (self, packet);

  else
    g_assert_not_reached ();
}

/*
 * GObject
 */
static void
valent_notification_plugin_class_init (ValentNotificationPluginClass *klass)
{
  ValentDevicePluginClass *plugin_class = VALENT_DEVICE_PLUGIN_CLASS (klass);

  plugin_class->enable = valent_notification_plugin_enable;
  plugin_class->disable = valent_notification_plugin_disable;
  plugin_class->handle_packet = valent_notification_plugin_handle_packet;
  plugin_class->update_state = valent_notification_plugin_update_state;
}

static void
valent_notification_plugin_init (ValentNotificationPlugin *self)
{
}

