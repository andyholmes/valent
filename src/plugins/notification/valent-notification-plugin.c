// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-notification-plugin"

#include "config.h"

#include <gtk/gtk.h>
#include <libpeas/peas.h>
#include <libvalent-core.h>
#include <libvalent-notifications.h>
#include <libvalent-session.h>

#include "valent-notification-plugin.h"


struct _ValentNotificationPlugin
{
  PeasExtensionBase    parent_instance;

  ValentDevice        *device;
  GSettings           *settings;

  ValentNotifications *notifications;
  gulong               notifications_id;

  ValentSession       *session;
};

static void valent_device_plugin_iface_init (ValentDevicePluginInterface *iface);

G_DEFINE_TYPE_WITH_CODE (ValentNotificationPlugin, valent_notification_plugin, PEAS_TYPE_EXTENSION_BASE,
                         G_IMPLEMENT_INTERFACE (VALENT_TYPE_DEVICE_PLUGIN, valent_device_plugin_iface_init))

enum {
  PROP_0,
  PROP_DEVICE,
  N_PROPERTIES
};

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
  const char *application;
  g_auto (GStrv) deny = NULL;

  g_assert (VALENT_IS_NOTIFICATIONS (listener));
  g_assert (VALENT_IS_NOTIFICATION (notification));
  g_assert (VALENT_IS_NOTIFICATION_PLUGIN (self));

  if (!g_settings_get_boolean (self->settings, "forward-notifications"))
    return;

  if (!g_settings_get_boolean (self->settings, "forward-when-active") &&
      !valent_session_get_active (self->session))
    return;

  application = valent_notification_get_application (notification);
  deny = g_settings_get_strv (self->settings, "forward-deny");

  if (application && g_strv_contains ((const char * const *)deny, application))
    return;

  if (!valent_device_get_connected (self->device))
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
  g_assert (VALENT_IS_NOTIFICATIONS (listener));
  g_assert (id != NULL);

  valent_notification_plugin_close_notification (self, id);
}

/*
 * GIcon Helpers
 */
static GtkIconTheme *
get_icon_theme (void)
{
  GdkDisplay *display;

  if (!gtk_is_initialized ())
    return NULL;

  if ((display = gdk_display_get_default ()) == NULL)
    return NULL;

  return gtk_icon_theme_get_for_display (display);
}

static GFile *
get_icon_gfile (ValentNotificationPlugin *self,
                const char               *filehash)
{
  ValentData *data;
  g_autofree char *cache_dir = NULL;

  data = valent_device_get_data (self->device);
  cache_dir = g_build_filename (valent_data_get_cache_path (data),
                                "notification",
                                NULL);

  if (!g_file_test (cache_dir, G_FILE_TEST_IS_DIR))
    g_mkdir_with_parents (cache_dir, 0755);

  return g_file_new_build_filename (cache_dir, filehash, NULL);
}

static int
get_largest_icon (GtkIconTheme *theme,
                  const char   *name)
{
  g_autofree int *sizes = NULL;
  int ret = 0;

  g_assert (GTK_IS_ICON_THEME (theme));
  g_assert (name != NULL);

  sizes = gtk_icon_theme_get_icon_sizes (theme, name);

  for (unsigned int i = 0; sizes[i] != 0; i++)
    {
      if (sizes[i] == -1)
        return -1;

      if (sizes[i] > ret)
        ret = sizes[i];
    }

  return ret;
}

static GFile *
get_largest_icon_file (GIcon *icon)
{
  GtkIconTheme *theme;
  const char * const *names;
  g_autoptr (GtkIconPaintable) info = NULL;

  g_assert (G_IS_THEMED_ICON (icon));

  if ((theme = get_icon_theme ()) == NULL)
    return NULL;

  names = g_themed_icon_get_names (G_THEMED_ICON (icon));

  for (unsigned int i = 0; names[i]; i++)
    {
      int size;

      if ((size = get_largest_icon (theme, names[i])) == 0)
        continue;

      info = gtk_icon_theme_lookup_by_gicon (theme, icon, size, 1, GTK_TEXT_DIR_NONE, 0);

      if (info != NULL)
        return gtk_icon_paintable_get_file (info);
    }

  return NULL;
}

static void
on_size_prepared (GdkPixbufLoader *loader,
                  gint             width,
                  gint             height,
                  gpointer         user_data)
{
  GdkPixbufFormat *format = gdk_pixbuf_loader_get_format (loader);

  if (gdk_pixbuf_format_is_scalable (format))
    gdk_pixbuf_loader_set_size (loader, 512, 512);
}

static GBytes *
icon_to_png_bytes (GIcon         *icon,
                   GCancellable  *cancellable,
                   GError       **error)
{
  g_autoptr (GBytes) bytes = NULL;
  g_autoptr (GdkPixbufLoader) loader = NULL;
  g_autoptr (GdkPixbuf) pixbuf = NULL;
  char *data;
  gsize size;

  g_assert (G_IS_ICON (icon));

  if (G_IS_THEMED_ICON (icon))
    {
      g_autoptr (GFile) file = NULL;

      file = get_largest_icon_file (icon);

      if (file == NULL)
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_FAILED,
                       "Failed to load themed icon");
          return NULL;
        }

      bytes = g_file_load_bytes (file, cancellable, NULL, error);
    }
  else if (G_IS_FILE_ICON (icon))
    {
      GFile *file;

      file = g_file_icon_get_file (G_FILE_ICON (icon));
      bytes = g_file_load_bytes (file, cancellable, NULL, error);
    }
  else if (G_IS_BYTES_ICON (icon))
    {
      GBytes *buffer;

      buffer = g_bytes_icon_get_bytes (G_BYTES_ICON (icon));

      if (buffer == NULL)
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_FAILED,
                       "Failed to load bytes from icon");
          return NULL;
        }

      bytes = g_bytes_ref (buffer);
    }

  if (bytes == NULL)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Failed to load bytes from icon");
      return NULL;
    }

  /* Try to load the icon bytes */
  loader = gdk_pixbuf_loader_new ();
  g_signal_connect (G_OBJECT (loader),
                    "size-prepared",
                    G_CALLBACK (on_size_prepared),
                    NULL);

  if (!gdk_pixbuf_loader_write_bytes (loader, bytes, error))
    return NULL;

  if (!gdk_pixbuf_loader_close (loader, error))
    return NULL;

  if ((pixbuf = gdk_pixbuf_loader_get_pixbuf (loader)) == NULL)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Failed to create pixbuf from bytes");
      return NULL;
    }

  if (!gdk_pixbuf_save_to_buffer (pixbuf, &data, &size, "png", error, NULL))
    return NULL;

  return g_bytes_new_take (data, size);
}

/*
 * Icon Transfers
 */
static void
download_icon_task (GTask        *task,
                    gpointer      source_object,
                    gpointer      task_data,
                    GCancellable *cancellable)
{
  ValentNotificationPlugin *self = source_object;
  g_autoptr (GFile) file = NULL;
  JsonNode *packet = task_data;
  const char *payload_hash;
  JsonObject *body;
  GError *error = NULL;

  if (g_task_return_error_if_cancelled (task))
    return;

  /* Check for a payload hash */
  body = json_node_get_object (packet);

  if ((payload_hash = valent_packet_check_string (body, "payloadHash")) != NULL)
    {
      file = get_icon_gfile (self, payload_hash);
    }
  else
    {
      g_autofree char *uuid = NULL;

      uuid = g_uuid_string_random ();
      file = get_icon_gfile (self, uuid);
    }

  /* Check if we've already downloaded this icon */
  if (!g_file_query_exists (file, cancellable))
    {
      g_autoptr (GIOStream) source = NULL;
      g_autoptr (GFileOutputStream) target = NULL;
      ValentChannel *channel;

      /* Get the device channel */
      if ((channel = valent_device_get_channel (self->device)) == NULL)
        {
          return g_task_return_new_error (task,
                                          G_IO_ERROR,
                                          G_IO_ERROR_CONNECTION_CLOSED,
                                          "Device is disconnected");
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

      source = valent_channel_download (channel, packet, cancellable, &error);

      if (source == NULL)
        {
          g_file_delete (file, NULL, NULL);
          return g_task_return_error (task, error);
        }

      /* Start download */
      g_output_stream_splice (G_OUTPUT_STREAM (target),
                              g_io_stream_get_input_stream (source),
                              (G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE |
                               G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET),
                              NULL,
                              &error);

      if (error != NULL)
        {
          g_file_delete (file, NULL, NULL);
          return g_task_return_error (task, error);
        }
    }

  /* If we're in a flatpak we send the file as a GBytesIcon */
  if (valent_in_flatpak ())
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
                                          GAsyncReadyCallback       callback,
                                          GCancellable             *cancellable,
                                          gpointer                  user_data)
{
  g_autoptr (GTask) task = NULL;

  g_return_if_fail (VALENT_IS_PACKET (packet));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_notification_plugin_download_icon);
  g_task_set_task_data (task,
                        json_node_ref (packet),
                        (GDestroyNotify)json_node_unref);
  g_task_run_in_thread (task, download_icon_task);
}

static GIcon *
valent_notification_plugin_download_icon_finish (ValentNotificationPlugin  *plugin,
                                                 GAsyncResult              *result,
                                                 GError                   **error)
{
  g_return_val_if_fail (g_task_is_valid (result, plugin), NULL);

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
  JsonObject *body;
  g_autoptr (GNotification) notification = NULL;
  g_autoptr (GIcon) icon = NULL;
  g_auto (GStrv) ticker_strv = NULL;
  const char *id;
  const char *napp = NULL;
  const char *title = NULL;
  const char *text = NULL;
  const char *ticker;
  const char *reply_id;

  body = valent_packet_get_body (packet);

  /* Finish the icon task */
  if (G_IS_ICON (gicon))
    icon = g_object_ref (gicon);

  /* Ensure we have a notification id */
  if ((id = valent_packet_check_string (body, "id")) == NULL)
    {
      g_warning ("%s: missing \"id\" field", G_STRFUNC);
      return;
    }

  /* This should never be absent, but we check anyways */
  if (json_object_has_member (body, "appName"))
    napp = json_object_get_string_member (body, "appName");

  /* Prefer `title` & `text` */
  title = valent_packet_check_string (body, "title");
  text = valent_packet_check_string (body, "text");

  /* Try to compose `title` & `text` from ticker */
  if ((title == NULL || text == NULL) &&
      (ticker = valent_packet_check_string (body, "ticker")) != NULL)
    {
      ticker_strv = g_strsplit (ticker, ": ", 2);
      title = ticker_strv[0];
      text = ticker_strv[1];
    }

  if (title == NULL || text == NULL)
    {
      g_warning ("Missing title/text and ticker");
      return;
    }

  /* Start building the GNotification */
  notification = g_notification_new (title);

  /* Repliable Notification */
  if ((reply_id = valent_packet_check_string (body, "requestReplyId")) != NULL)
    {
      g_autoptr (GError) rerror = NULL;
      JsonNode *node;
      GVariant *parameters;

      node = json_object_get_member (json_node_get_object (packet), "body");
      parameters = json_gvariant_deserialize (node, "a{sv}", &rerror);

      if (parameters != NULL)
        {
          GVariant *target;

          target = g_variant_new ("(ssv)", reply_id, "", parameters);
          valent_notification_set_device_action (notification,
                                                 self->device,
                                                 "notification-reply",
                                                 target);
        }
      else
        g_debug ("%s: deserializing JSON node: %s", G_STRFUNC, rerror->message);
    }

  /* Notification Actions */
  if (json_object_has_member (body, "actions"))
    {
      JsonArray *actions;
      unsigned int n_actions;

      actions = json_object_get_array_member (body, "actions");
      n_actions = json_array_get_length (actions);

      for (unsigned int i = 0; i < n_actions; i++)
        {
          const char *action;
          GVariant *target;

          action = json_array_get_string_element (actions, i);
          target = g_variant_new ("(ss)", id, action);
          valent_notification_add_device_button (notification,
                                                 self->device,
                                                 action,
                                                 "notification-action",
                                                 target);
        }
    }

  /* Special case for missed calls */
  if (g_strrstr (id, "MissedCall") != NULL)
    {
      g_notification_set_title (notification, title);
      g_notification_set_body (notification, text);

      if (icon == NULL)
        icon = g_themed_icon_new ("call-missed-symbolic");
    }

  /* Special case for SMS messages */
  else if (g_strrstr (id, "sms") != NULL)
    {
      g_notification_set_title (notification, title);
      g_notification_set_body (notification, text);

      if (icon == NULL)
        icon = g_themed_icon_new ("sms-symbolic");
    }

  /* Ignore `appName` if it's the same as `title` */
  else if (g_strcmp0 (napp, title) == 0)
    {
      g_notification_set_title (notification, title);
      g_notification_set_body (notification, text);
    }

  /* Fallback to ticker-style */
  else
    {
      g_autofree char *ticker = NULL;

      ticker = g_strdup_printf ("%s: %s", title, text);
      g_notification_set_title (notification, napp);
      g_notification_set_body (notification, ticker);
    }

  if (icon != NULL)
    g_notification_set_icon (notification, icon);

  valent_device_show_notification (self->device, id, notification);
}

static void
download_cb (ValentNotificationPlugin *self,
             GAsyncResult             *result,
             gpointer                  user_data)
{
  g_autoptr (GError) error = NULL;
  g_autoptr (JsonNode) packet = user_data;
  g_autoptr (GIcon) icon = NULL;

  g_return_if_fail (g_task_is_valid (result, self));

  /* Finish the icon task */
  icon = valent_notification_plugin_download_icon_finish (self, result, &error);

  if (icon == NULL)
    {
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
  JsonObject *body;

  g_assert (VALENT_IS_NOTIFICATION_PLUGIN (self));
  g_assert (VALENT_IS_PACKET (packet));

  body = valent_packet_get_body (packet);

  /* A report that a remote notification has been dismissed */
  if (json_object_get_boolean_member_with_default (body, "isCancel", FALSE))
    {
      const char *id;

      if ((id = valent_packet_check_string (body, "id")) != NULL)
        valent_device_hide_notification (self->device, id);

      return;
    }

  /* A notification that should only be shown once */
  if (json_object_get_boolean_member_with_default (body, "onlyOnce", FALSE))
    {
      VALENT_TODO ("Handle 'onlyOnce' field");
    }

  /* A notification with an icon payload */
  if (valent_packet_has_payload (packet))
    {
      valent_notification_plugin_download_icon (self,
                                                packet,
                                                (GAsyncReadyCallback)download_cb,
                                                NULL,
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
  JsonBuilder *builder;
  g_autoptr (JsonNode) packet = NULL;

  g_assert (VALENT_IS_NOTIFICATION_PLUGIN (self));

  builder = valent_packet_start ("kdeconnect.notification.request");
  json_builder_set_member_name (builder, "request");
  json_builder_add_boolean_value (builder, TRUE);
  packet = valent_packet_finish (builder);

  valent_device_queue_packet (self->device, packet);
}

static void
valent_notification_plugin_close_notification (ValentNotificationPlugin *self,
                                               const char               *id)
{
  JsonBuilder *builder;
  g_autoptr (JsonNode) packet = NULL;

  g_assert (VALENT_IS_NOTIFICATION_PLUGIN (self));
  g_assert (id != NULL);

  builder = valent_packet_start ("kdeconnect.notification.request");
  json_builder_set_member_name (builder, "cancel");
  json_builder_add_string_value (builder, id);
  packet = valent_packet_finish (builder);

  valent_device_queue_packet (self->device, packet);
}

static void
valent_notification_plugin_send_notification_with_icon (ValentNotificationPlugin *self,
                                                        JsonNode                 *packet,
                                                        GIcon                    *icon)
{
  g_autoptr (ValentTransfer) transfer = NULL;

  g_assert (VALENT_IS_NOTIFICATION_PLUGIN (self));
  g_assert (VALENT_IS_PACKET (packet));
  g_assert (icon == NULL || G_IS_ICON (icon));

  transfer = valent_transfer_new (self->device);

  /* Try to ensure icons are sent in PNG format, since kdeconnect-android can't
   * handle SVGs which are very common */
  if (G_IS_ICON (icon))
    {
      g_autoptr (GBytes) bytes = NULL;
      g_autoptr (GError) error = NULL;

      bytes = icon_to_png_bytes (icon, NULL, &error);

      if (bytes != NULL)
        {
          valent_transfer_add_bytes (transfer, packet, bytes);
          valent_transfer_execute (transfer, NULL, NULL, NULL);
          return;
        }
      else
        g_debug ("Converting icon to PNG: %s", error->message);
    }

  // FIXME: convert to GFile -> GBytes
  if (G_IS_BYTES_ICON (icon))
    {
      GBytes *bytes;

      if ((bytes = g_bytes_icon_get_bytes (G_BYTES_ICON (icon))) != NULL)
        {
          valent_transfer_add_bytes (transfer, packet, bytes);
          valent_transfer_execute (transfer, NULL, NULL, NULL);
          return;
        }
    }
  else if (G_IS_FILE_ICON (icon))
    {
      GFile *file;

      file = g_file_icon_get_file (G_FILE_ICON (icon));
      valent_transfer_add_file (transfer, packet, file);
      valent_transfer_execute (transfer, NULL, NULL, NULL);
      return;
    }
  else if (G_IS_THEMED_ICON (icon))
    {
      GtkIconTheme *theme;

      if ((theme = get_icon_theme ()) != NULL)
        {
          g_autoptr (GtkIconPaintable) info = NULL;

          info = gtk_icon_theme_lookup_by_gicon (theme, icon, -1, 1, GTK_TEXT_DIR_NONE, 0);

          if (info != NULL)
            {
              g_autoptr (GFile) file = NULL;

              file = gtk_icon_paintable_get_file (info);
              valent_transfer_add_file (transfer, packet, file);
              valent_transfer_execute (transfer, NULL, NULL, NULL);
              return;
            }
        }
    }

  valent_device_queue_packet (self->device, packet);
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
  JsonBuilder *builder;
  g_autoptr (JsonNode) packet = NULL;
  g_autofree char *ticker = NULL;

  g_return_if_fail (VALENT_IS_NOTIFICATION_PLUGIN (self));
  g_return_if_fail (id != NULL);
  g_return_if_fail (icon == NULL || G_IS_ICON (icon));

  /* Build packet */
  builder = valent_packet_start ("kdeconnect.notification");
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

  packet = valent_packet_finish (builder);

  if (icon == NULL)
    valent_device_queue_packet (self->device, packet);
  else
    valent_notification_plugin_send_notification_with_icon (self, packet, icon);
}

/*
 * GActions
 */

/**
 * notificaton-action:
 * @parameter: "(ss)": a tuple of @id and @name
 *
 * Inform the remote device that action @name on the remote notification @id has
 * been activated by the user.
 */
static void
notification_action_action (GSimpleAction *action,
                            GVariant      *parameter,
                            gpointer       user_data)
{
  ValentNotificationPlugin *self = VALENT_NOTIFICATION_PLUGIN (user_data);
  JsonBuilder *builder;
  g_autoptr (JsonNode) packet = NULL;
  char *id;
  char *name;

  g_assert (VALENT_IS_NOTIFICATION_PLUGIN (self));

  g_variant_get (parameter, "(&s&s)", &id, &name);

  builder = valent_packet_start ("kdeconnect.notification.action");
  json_builder_set_member_name (builder, "key");
  json_builder_add_string_value (builder, id);
  json_builder_set_member_name (builder, "action");
  json_builder_add_string_value (builder, name);
  packet = valent_packet_finish (builder);

  valent_device_queue_packet (self->device, packet);
}

/**
 * notificaton-cancel:
 * @parameter: "s": the remote notification id
 *
 * Inform the remote device the local notification @id has been withdrawn.
 */
static void
notification_cancel_action (GSimpleAction *action,
                            GVariant      *parameter,
                            gpointer       user_data)
{
  ValentNotificationPlugin *self = VALENT_NOTIFICATION_PLUGIN (user_data);
  const char *id;
  JsonBuilder *builder;
  g_autoptr (JsonNode) packet = NULL;

  g_assert (VALENT_IS_NOTIFICATION_PLUGIN (self));

  id = g_variant_get_string (parameter, NULL);

  builder = valent_packet_start ("kdeconnect.notification");
  json_builder_set_member_name (builder, "id");
  json_builder_add_string_value (builder, id);
  json_builder_set_member_name (builder, "isCancel");
  json_builder_add_boolean_value (builder, TRUE);
  packet = valent_packet_finish (builder);

  valent_device_queue_packet (self->device, packet);
}

/**
 * notificaton-close:
 * @parameter: "s": the remote notification id
 *
 * Request the remote device close the remote notification @id.
 */
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

/**
 * notificaton-reply:
 * @parameter: "s": the remote notification id
 *
 * Request the remote device close the remote notification @id.
 */
static void
notification_reply_action (GSimpleAction *action,
                           GVariant      *parameter,
                           gpointer       user_data)
{
  ValentNotificationPlugin *self = VALENT_NOTIFICATION_PLUGIN (user_data);
  const char *reply_id;
  const char *message;
  g_autoptr (GVariant) notification = NULL;

  g_assert (VALENT_IS_NOTIFICATION_PLUGIN (self));

  g_variant_get (parameter, "(&s&sv)", &reply_id, &message, &notification);

  if (strlen (message) == 0)
    {
      // TODO: reply dialog
    }
  else
    {
      JsonBuilder *builder;
      g_autoptr (JsonNode) packet = NULL;

      builder = valent_packet_start ("kdeconnect.notification.reply");
      json_builder_set_member_name (builder, "requestReplyId");
      json_builder_add_string_value (builder, reply_id);
      json_builder_set_member_name (builder, "message");
      json_builder_add_string_value (builder, message);
      packet = valent_packet_finish (builder);

      valent_device_queue_packet (self->device, packet);
    }
}

/**
 * notificaton-send:
 * @parameter: "s": the remote notification id
 *
 * Request the remote device close the remote notification @id.
 */
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
    {"notification-action", notification_action_action, "(ss)",  NULL, NULL},
    {"notification-cancel", notification_cancel_action, "s",     NULL, NULL},
    {"notification-close",  notification_close_action,  "s",     NULL, NULL},
    {"notification-reply",  notification_reply_action,  "(ssv)", NULL, NULL},
    {"notification-send",   notification_send_action,   "a{sv}", NULL, NULL},
};

/**
 * ValentDevicePlugin
 */
static void
valent_notification_plugin_enable (ValentDevicePlugin *plugin)
{
  ValentNotificationPlugin *self = VALENT_NOTIFICATION_PLUGIN (plugin);
  const char *device_id;

  g_assert (VALENT_IS_NOTIFICATION_PLUGIN (self));

  /* Setup GSettings */
  device_id = valent_device_get_id (self->device);
  self->settings = valent_device_plugin_new_settings (device_id, "notification");

  /* Register GActions */
  valent_device_plugin_register_actions (plugin,
                                         actions,
                                         G_N_ELEMENTS (actions));

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
}

static void
valent_notification_plugin_disable (ValentDevicePlugin *plugin)
{
  ValentNotificationPlugin *self = VALENT_NOTIFICATION_PLUGIN (plugin);

  /* Stop watching for local notifications */
  if (self->notifications)
    g_signal_handlers_disconnect_by_data (self->notifications, self);

  /* Unregister GActions */
  valent_device_plugin_unregister_actions (plugin,
                                           actions,
                                           G_N_ELEMENTS (actions));

  /* Dispose GSettings */
  g_clear_object (&self->settings);
}

static void
valent_notification_plugin_update_state (ValentDevicePlugin *plugin)
{
  ValentNotificationPlugin *self = VALENT_NOTIFICATION_PLUGIN (plugin);
  gboolean connected;
  gboolean paired;
  gboolean available;

  connected = valent_device_get_connected (self->device);
  paired = valent_device_get_paired (self->device);
  available = (connected && paired);

  /* GActions */
  valent_device_plugin_toggle_actions (plugin,
                                       actions, G_N_ELEMENTS (actions),
                                       available);

  /* Request Notifications */
  if (available)
    valent_notification_plugin_request_notifications (self);

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

  if (g_strcmp0 (type, "kdeconnect.notification") == 0)
    valent_notification_plugin_handle_notification (self, packet);

  else if (g_strcmp0 (type, "kdeconnect.notification.action") == 0)
    valent_notification_plugin_handle_notification_action (self, packet);

  else if (g_strcmp0 (type, "kdeconnect.notification.reply") == 0)
    valent_notification_plugin_handle_notification_reply (self, packet);

  else if (g_strcmp0 (type, "kdeconnect.notification.request") == 0)
    valent_notification_plugin_handle_notification_request (self, packet);

  else
    g_assert_not_reached ();
}

static void
valent_device_plugin_iface_init (ValentDevicePluginInterface *iface)
{
  iface->enable = valent_notification_plugin_enable;
  iface->disable = valent_notification_plugin_disable;
  iface->handle_packet = valent_notification_plugin_handle_packet;
  iface->update_state = valent_notification_plugin_update_state;
}

/**
 * GObject
 */
static void
valent_notification_plugin_get_property (GObject    *object,
                                         guint       prop_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
  ValentNotificationPlugin *self = VALENT_NOTIFICATION_PLUGIN (object);

  switch (prop_id)
    {
    case PROP_DEVICE:
      g_value_set_object (value, self->device);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_notification_plugin_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  ValentNotificationPlugin *self = VALENT_NOTIFICATION_PLUGIN (object);

  switch (prop_id)
    {
    case PROP_DEVICE:
      self->device = g_value_get_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_notification_plugin_class_init (ValentNotificationPluginClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = valent_notification_plugin_get_property;
  object_class->set_property = valent_notification_plugin_set_property;

  g_object_class_override_property (object_class, PROP_DEVICE, "device");
}

static void
valent_notification_plugin_init (ValentNotificationPlugin *self)
{
}

