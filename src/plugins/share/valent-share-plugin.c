// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-share-plugin"

#include "config.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <json-glib/json-glib.h>
#include <libpeas/peas.h>
#include <libvalent-core.h>

#include "valent-share-plugin.h"


struct _ValentSharePlugin
{
  PeasExtensionBase  parent_instance;

  ValentDevice      *device;

  GHashTable        *transfers;
};

static void valent_device_plugin_iface_init (ValentDevicePluginInterface *iface);

G_DEFINE_TYPE_WITH_CODE (ValentSharePlugin, valent_share_plugin, PEAS_TYPE_EXTENSION_BASE,
                         G_IMPLEMENT_INTERFACE (VALENT_TYPE_DEVICE_PLUGIN, valent_device_plugin_iface_init))

enum {
  PROP_0,
  PROP_DEVICE,
  N_PROPERTIES
};


/*
 * File Downloads
 */
typedef struct
{
  ValentSharePlugin *plugin;
  GFile             *file;
  unsigned int       open : 1;
} DownloadOperation;

static void
download_file_cb (ValentTransfer *transfer,
                  GAsyncResult   *result,
                  gpointer        user_data)
{
  g_autofree DownloadOperation *op = user_data;
  g_autoptr (ValentSharePlugin) self = g_steal_pointer (&op->plugin);
  g_autoptr (GFile) file = g_steal_pointer (&op->file);
  g_autoptr (GError) error = NULL;

  g_autoptr (GNotification) notif = NULL;
  g_autoptr (GIcon) icon = NULL;
  g_autofree char *body = NULL;

  g_autoptr (GFile) dir = NULL;
  g_autofree char *diruri = NULL;
  g_autofree char *filename = NULL;
  g_autofree char *fileuri = NULL;

  g_assert (VALENT_IS_TRANSFER (transfer));
  g_assert (G_IS_FILE (file));
  g_assert (VALENT_IS_SHARE_PLUGIN (self));

  /* Prepare notification */
  if (valent_transfer_execute_finish (transfer, result, &error))
    {
      dir = g_file_get_parent (file);
      diruri = g_file_get_uri (dir);
      filename = g_file_get_basename (file);
      fileuri = g_file_get_uri (file);

      notif = g_notification_new (_("Transfer Complete"));
      icon = g_themed_icon_new ("document-save-symbolic");
      body = g_strdup_printf (_("Received “%s” from %s"),
                              filename,
                              valent_device_get_name (self->device));

      valent_notification_add_device_button (notif,
                                             self->device,
                                             _("Open Folder"),
                                             "share-open",
                                             g_variant_new_string (diruri));

      valent_notification_add_device_button (notif,
                                             self->device,
                                             _("Open File"),
                                             "share-open",
                                             g_variant_new_string (fileuri));
    }
  else if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
      filename = g_file_get_basename (file);

      notif = g_notification_new (_("Transfer Failed"));
      icon = g_themed_icon_new ("dialog-error-symbolic");
      body = g_strdup_printf (_("Failed to receive “%s” from %s"),
                              filename,
                              valent_device_get_name (self->device));
    }

  /* Send new notification */
  valent_device_hide_notification (self->device, valent_transfer_get_id (transfer));

  if (notif != NULL)
    {
      g_notification_set_body (notif, body);
      g_notification_set_icon (notif, icon);
      valent_device_show_notification (self->device,
                                       valent_transfer_get_id (transfer),
                                       notif);
    }

  g_hash_table_remove (self->transfers, valent_transfer_get_id (transfer));
}

static void
download_file (ValentSharePlugin *self,
               GFile             *file,
               JsonNode          *packet)
{
  g_autoptr (ValentTransfer) transfer = NULL;
  const char *uuid;
  g_autofree char *filename = NULL;
  DownloadOperation *op;

  g_autoptr (GNotification) notif = NULL;
  g_autoptr (GIcon) icon = NULL;
  const char *title;
  g_autofree char *body = NULL;

  g_assert (VALENT_IS_SHARE_PLUGIN (self));
  g_assert (G_IS_FILE (file));
  g_assert (VALENT_IS_PACKET (packet));

  /* Operation Data */
  transfer = valent_transfer_new (self->device);
  uuid = valent_transfer_get_id (transfer);

  op = g_new0 (DownloadOperation, 1);
  op->plugin = g_object_ref (self);
  op->file = g_file_dup (file);
  op->open = FALSE;

  /* Notify the user */
  filename = g_file_get_basename (file);
  icon = g_themed_icon_new ("document-save-symbolic");
  title = _("Transferring File");
  body = g_strdup_printf (_("Receiving “%s” from %s"),
                          filename,
                          valent_device_get_name (self->device));

  notif = g_notification_new (title);
  g_notification_set_body (notif, body);
  g_notification_set_icon (notif, icon);
  valent_notification_add_device_button (notif,
                                         self->device,
                                         _("Cancel"),
                                         "share-cancel",
                                         g_variant_new_string (uuid));

  valent_device_show_notification (self->device, uuid, notif);

  /* Track and start the transfer */
  g_hash_table_insert (self->transfers,
                       g_strdup (uuid),
                       g_object_ref (transfer));

  valent_transfer_add_file (transfer, packet, file);
  valent_transfer_execute (transfer,
                           NULL,
                           (GAsyncReadyCallback)download_file_cb,
                           op);
}


/*
 * File Uploads
 */
typedef struct
{
  ValentSharePlugin *plugin;
  GListModel        *files;
} UploadOperation;

static void
upload_files_cb (ValentTransfer *transfer,
                 GAsyncResult   *result,
                 gpointer        user_data)
{
  g_autofree UploadOperation *op = user_data;
  g_autoptr (ValentSharePlugin) self = g_steal_pointer (&op->plugin);
  g_autoptr (GListModel) files = g_steal_pointer (&op->files);
  g_autoptr (GError) error = NULL;
  unsigned int n_files;
  const char *uuid;

  g_autoptr (GNotification) notif = NULL;
  g_autoptr (GIcon) icon = NULL;
  const char *title;
  g_autofree char *body = NULL;

  n_files = g_list_model_get_n_items (files);
  uuid = valent_transfer_get_id (transfer);

  if (valent_transfer_execute_finish (transfer, result, &error))
    {
      icon = g_themed_icon_new ("document-send-symbolic");
      title = _("Transfer Successful");

      if (n_files == 1)
        {
          g_autoptr (GFile) file = NULL;
          g_autofree char *filename = NULL;

          file = g_list_model_get_item (files, 0);
          filename = g_file_get_basename (file);
          body = g_strdup_printf (_("Sent “%s” to %s"),
                                  filename,
                                  valent_device_get_name (self->device));
        }
      else
        {
          body = g_strdup_printf ("Uploaded %u files to %s",
                                  n_files,
                                  valent_device_get_name (self->device));
        }
    }
  else if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
      icon = g_themed_icon_new ("dialog-error-symbolic");
      title = _("Transfer Failed");

      if (n_files == 1)
        {
          g_autoptr (GFile) file = NULL;
          g_autofree char *filename = NULL;

          file = g_list_model_get_item (files, 0);
          filename = g_file_get_basename (file);
          body = g_strdup_printf (_("Sending “%s” to %s: %s"),
                                  filename,
                                  valent_device_get_name (self->device),
                                  error->message);
        }
      else
        {
          body = g_strdup_printf ("Sending %u files to %s: %s",
                                  n_files,
                                  valent_device_get_name (self->device),
                                  error->message);
        }
    }

  /* Update the notification */
  valent_device_hide_notification (self->device, uuid);

  if (icon != NULL)
    {
      notif = g_notification_new (title);
      g_notification_set_body (notif, body);
      g_notification_set_icon (notif, icon);
      valent_device_show_notification (self->device, uuid, notif);
    }

  g_hash_table_remove (self->transfers, uuid);
}

static void
upload_files (ValentSharePlugin *self,
              GListModel        *files,
              gboolean           open)
{
  UploadOperation *op;
  g_autoptr (ValentTransfer) transfer = NULL;
  g_autoptr (GNotification) notif = NULL;
  g_autoptr (GIcon) icon = NULL;
  const char *title;
  g_autofree char *body = NULL;
  const char *uuid;
  unsigned int n_files;
  JsonBuilder *builder;

  op = g_new0 (UploadOperation, 1);
  op->plugin = g_object_ref (self);
  op->files = g_object_ref (files);

  /* Track the transfer's cancellable */
  transfer = valent_transfer_new (self->device);
  n_files = g_list_model_get_n_items (files);

  /* Add files */
  for (unsigned int i = 0; i < n_files; i++)
    {
      g_autoptr (JsonNode) packet = NULL;
      g_autoptr (GFile) file = g_list_model_get_item (files, i);
      g_autofree char *filename = NULL;

      /* Build Packet */
      builder = valent_packet_start ("kdeconnect.share.request");

      filename = g_file_get_basename (file);
      json_builder_set_member_name (builder, "filename");
      json_builder_add_string_value (builder, filename);

      // TODO
      json_builder_set_member_name (builder, "open");
      json_builder_add_boolean_value (builder, FALSE);

      packet = valent_packet_finish (builder);

      /* Append transfer */
      valent_transfer_add_file (transfer, packet, file);
    }

  /* Notify the user */
  uuid = valent_transfer_get_id (transfer);
  title = _("Transferring Files");
  body = g_strdup (_("Transferring Files"));
  icon = g_themed_icon_new ("document-send-symbolic");

  notif = g_notification_new (title);
  g_notification_set_body (notif, body);
  g_notification_set_icon (notif, icon);
  valent_notification_add_device_button (notif,
                                         self->device,
                                         _("Cancel"),
                                         "share-cancel",
                                         g_variant_new_string (uuid));

  valent_device_show_notification (self->device, uuid, notif);

  /* Start the transfer */
  g_hash_table_insert (self->transfers,
                       g_strdup (uuid),
                       g_object_ref (transfer));
  valent_transfer_execute (transfer,
                           NULL,
                           (GAsyncReadyCallback)upload_files_cb,
                           op);
}

/*
 * GActions
 */
static void
share_response (GtkNativeDialog *dialog,
                gint             response_id,
                gpointer         user_data)
{
  ValentSharePlugin *self = VALENT_SHARE_PLUGIN (user_data);
  g_autoptr (GListModel) files = NULL;

  if (response_id == GTK_RESPONSE_ACCEPT)
    {
      files = gtk_file_chooser_get_files (GTK_FILE_CHOOSER (dialog));
      upload_files (self, files, FALSE);
    }

  gtk_native_dialog_destroy (dialog);
}

/**
 * share:
 * @parameter: %NULL
 *
 * The default share action opens a #GtkFileChooserDialog for selecting files.
 * It is the most reasonable user-visible action.
 */
static void
share_action (GSimpleAction *action,
              GVariant      *parameter,
              gpointer       user_data)
{
  ValentSharePlugin *self = VALENT_SHARE_PLUGIN (user_data);
  GtkFileChooserNative *dialog;

  g_assert (VALENT_IS_SHARE_PLUGIN (self));

  dialog = gtk_file_chooser_native_new (_("Share Files"),
                                        NULL,
                                        GTK_FILE_CHOOSER_ACTION_OPEN,
                                        _("Share"),
                                        _("Cancel"));

  gtk_file_chooser_set_select_multiple (GTK_FILE_CHOOSER (dialog), TRUE);

  g_signal_connect (dialog,
                    "response",
                    G_CALLBACK (share_response),
                    self);

  gtk_native_dialog_show (GTK_NATIVE_DIALOG (dialog));
}

/**
 * share-cancel:
 * @parameter: "s"
 * @uuid: The transfer UUID
 *
 * Each transfer is given a UUID for the purposes of cancelling it. Usually this
 * action will only be activated from the transfer notification as sent by
 * upload_operation() or the incoming file handler.
 */
static void
share_cancel_action (GSimpleAction *action,
                     GVariant      *parameter,
                     gpointer       user_data)
{
  ValentSharePlugin *self = VALENT_SHARE_PLUGIN (user_data);
  const char *uuid;
  ValentTransfer *transfer;

  g_assert (VALENT_IS_SHARE_PLUGIN (self));

  uuid = g_variant_get_string (parameter, NULL);
  transfer = g_hash_table_lookup (self->transfers, uuid);

  if (transfer != NULL)
    valent_transfer_cancel (transfer);
}

/**
 * share-files:
 * @parameter: "as"
 * @uris: A list of file URIs
 *
 * This is the best action to use for directly invoked uploads that still
 * require user notification and interaction.
 */
static void
share_files_action (GSimpleAction *action,
                    GVariant      *parameter,
                    gpointer       user_data)
{
  ValentSharePlugin *self = VALENT_SHARE_PLUGIN (user_data);
  g_autoptr (GListStore) files = NULL;
  const char **uris;
  gsize n_files;

  g_assert (VALENT_IS_SHARE_PLUGIN (self));

  files = g_list_store_new (G_TYPE_FILE);
  uris = g_variant_get_strv (parameter, &n_files);

  for (unsigned int i = 0; i < n_files; i++)
    {
      g_autoptr (GFile) file = NULL;

      file = g_file_new_for_uri (uris[i]);
      g_list_store_append (files, file);
    }
  g_free (uris);

  upload_files (self, G_LIST_MODEL (files), FALSE);
}

/**
 * share-open:
 * @parameter: "s"
 * @uri: File URI to open
 *
 * This action opens a file or directory.
 */
static void
share_open_action (GSimpleAction *action,
                   GVariant      *parameter,
                   gpointer       user_data)
{
  const char *uri;

  uri = g_variant_get_string (parameter, NULL);
  g_app_info_launch_default_for_uri_async (uri, NULL, NULL, NULL, NULL);
}

/**
 * share-text:
 * @parameter: "s"
 * @text: text to share
 *
 * This action simply sends a chunk of text to the remote device. Ultimately,
 * how the remote device handles the text is undefined. It may be copied to the
 * clipboard, stored as a temporary file or just displayed.
 */
static void
share_text_action (GSimpleAction *action,
                   GVariant      *parameter,
                   gpointer       user_data)
{
  ValentSharePlugin *self = VALENT_SHARE_PLUGIN (user_data);
  const char *text;
  JsonBuilder *builder;
  g_autoptr (JsonNode) packet = NULL;

  g_assert (VALENT_IS_SHARE_PLUGIN (self));

  text = g_variant_get_string (parameter, NULL);

  builder = valent_packet_start ("kdeconnect.share.request");
  json_builder_set_member_name (builder, "text");
  json_builder_add_string_value (builder, text);
  packet = valent_packet_finish (builder);

  valent_device_queue_packet (self->device, packet);
}

/**
 * share-url:
 * @parameter: "s"
 * @url: URL to share
 *
 * This action simply sends a URL as a string of text. By convention the remote
 * device will open the URL in the default browser.
 */
static void
share_url_action (GSimpleAction *action,
                  GVariant      *parameter,
                  gpointer       user_data)
{
  ValentSharePlugin *self = VALENT_SHARE_PLUGIN (user_data);
  const char *url;
  JsonBuilder *builder;
  g_autoptr (JsonNode) packet = NULL;

  g_assert (VALENT_IS_SHARE_PLUGIN (self));

  url = g_variant_get_string (parameter, NULL);

  builder = valent_packet_start ("kdeconnect.share.request");
  json_builder_set_member_name (builder, "url");
  json_builder_add_string_value (builder, url);
  packet = valent_packet_finish (builder);

  valent_device_queue_packet (self->device, packet);
}

static GActionEntry actions[] = {
    {"share",        share_action,        NULL, NULL, NULL},
    {"share-cancel", share_cancel_action, "s",  NULL, NULL},
    {"share-files",  share_files_action,  "as", NULL, NULL},
    {"share-open",   share_open_action,   "s",  NULL, NULL},
    {"share-text",   share_text_action,   "s",  NULL, NULL},
    {"share-url",    share_url_action,    "s",  NULL, NULL}
};

static const ValentMenuEntry items[] = {
    {N_("Send Files"), "device.share", "document-send-symbolic"}
};

/*
 * Packet Handlers
 */
static void
valent_share_plugin_handle_file (ValentSharePlugin *self,
                                 JsonNode          *packet)
{
  JsonObject *body;
  const char *filename;
  g_autoptr (GFile) file = NULL;

  g_assert (VALENT_IS_SHARE_PLUGIN (self));
  g_assert (VALENT_IS_PACKET (packet));

  body = valent_packet_get_body (packet);

  if (!valent_packet_has_payload (packet))
    {
      g_warning ("%s: missing payload info", G_STRFUNC);
      return;
    }

  if ((filename = valent_packet_check_string (body, "filename")) == NULL)
    {
      g_warning ("%s: invalid \"filename\" field", G_STRFUNC);
      return;
    }

  file = valent_device_new_download_file (self->device, filename, TRUE);

  download_file (self, file, packet);
}

static void
valent_share_plugin_handle_text (ValentSharePlugin *self,
                                 const char        *text)
{
  GtkMessageDialog *dialog;

  g_assert (VALENT_IS_SHARE_PLUGIN (self));
  g_assert (text != NULL);

  // TODO: fallback for headless
  if (!gtk_is_initialized ())
    return;

  dialog = g_object_new (GTK_TYPE_MESSAGE_DIALOG,
                         "text",           "Shared Text",
                         "secondary-text", text,
                         "buttons",        GTK_BUTTONS_CLOSE,
                         NULL);

  g_signal_connect (dialog,
                    "response",
                    G_CALLBACK (gtk_window_destroy),
                    NULL);

  gtk_window_present_with_time (GTK_WINDOW (dialog), GDK_CURRENT_TIME);
}

static void
valent_share_plugin_handle_url (ValentSharePlugin *self,
                                const char        *url)
{
  g_assert (VALENT_IS_SHARE_PLUGIN (self));
  g_assert (url != NULL);

  g_app_info_launch_default_for_uri_async (url, NULL, NULL, NULL, NULL);
}

/*
 * ValentDevicePlugin
 */
static void
valent_share_plugin_enable (ValentDevicePlugin *plugin)
{
  ValentSharePlugin *self = VALENT_SHARE_PLUGIN (plugin);

  g_assert (VALENT_IS_SHARE_PLUGIN (self));

  /* Prepare Transfer table */
  self->transfers = g_hash_table_new_full (g_str_hash, g_str_equal,
                                           g_free, g_object_unref);

  /* Register GActions */
  valent_device_plugin_register_actions (plugin,
                                         actions,
                                         G_N_ELEMENTS (actions));

  /* Register GMenu items */
  valent_device_plugin_add_menu_entries (plugin,
                                         items,
                                         G_N_ELEMENTS (items));
}

static void
valent_share_plugin_disable (ValentDevicePlugin *plugin)
{
  ValentSharePlugin *self = VALENT_SHARE_PLUGIN (plugin);
  GHashTableIter iter;
  gpointer transfer;

  g_assert (VALENT_IS_SHARE_PLUGIN (self));

  /* Unregister GMenu items */
  valent_device_plugin_remove_menu_entries (plugin,
                                            items,
                                            G_N_ELEMENTS (items));

  /* Unregister GActions */
  valent_device_plugin_unregister_actions (plugin,
                                           actions,
                                           G_N_ELEMENTS (actions));

  /* Cancel active transfers */
  g_hash_table_iter_init (&iter, self->transfers);

  while (g_hash_table_iter_next (&iter, NULL, &transfer))
    {
      valent_transfer_cancel (VALENT_TRANSFER (transfer));
      g_hash_table_iter_remove (&iter);
    }

  g_clear_pointer (&self->transfers, g_hash_table_unref);
}

static void
valent_share_plugin_handle_packet (ValentDevicePlugin *plugin,
                                   const char         *type,
                                   JsonNode           *packet)
{
  ValentSharePlugin *self = VALENT_SHARE_PLUGIN (plugin);
  JsonObject *body;

  g_assert (VALENT_IS_SHARE_PLUGIN (self));
  g_assert (type != NULL);
  g_assert (VALENT_IS_PACKET (packet));

  body = valent_packet_get_body (packet);

  if (g_strcmp0 (type, "kdeconnect.share.request") == 0)
    {
      if (json_object_has_member (body, "filename"))
        {
          valent_share_plugin_handle_file (self, packet);
        }
      else if (json_object_has_member (body, "text"))
        {
          const char *text;

          text = json_object_get_string_member (body, "text");
          valent_share_plugin_handle_text (self, text);
        }
      else if (json_object_has_member (body, "url"))
        {
          const char *url;

          url = json_object_get_string_member (body, "url");
          valent_share_plugin_handle_url (self, url);
        }

      else
        g_warning ("Share: unsupported share request");
    }
  else
    g_assert_not_reached ();
}

static void
valent_share_plugin_update_state (ValentDevicePlugin *plugin)
{
  ValentSharePlugin *self = VALENT_SHARE_PLUGIN (plugin);
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
}

static void
valent_device_plugin_iface_init (ValentDevicePluginInterface *iface)
{
  iface->enable = valent_share_plugin_enable;
  iface->disable = valent_share_plugin_disable;
  iface->handle_packet = valent_share_plugin_handle_packet;
  iface->update_state = valent_share_plugin_update_state;
}

/*
 * GObject
 */
static void
valent_share_plugin_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  ValentSharePlugin *self = VALENT_SHARE_PLUGIN (object);

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
valent_share_plugin_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  ValentSharePlugin *self = VALENT_SHARE_PLUGIN (object);

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
valent_share_plugin_class_init (ValentSharePluginClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = valent_share_plugin_get_property;
  object_class->set_property = valent_share_plugin_set_property;

  g_object_class_override_property (object_class, PROP_DEVICE, "device");
}

static void
valent_share_plugin_init (ValentSharePlugin *self)
{
}

