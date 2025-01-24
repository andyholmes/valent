// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-share-plugin"

#include "config.h"

#include <glib/gi18n.h>
#include <gio/gio.h>
#include <json-glib/json-glib.h>
#include <valent.h>

#include "valent-share-plugin.h"
#include "valent-share-download.h"
#include "valent-share-upload.h"


struct _ValentSharePlugin
{
  ValentDevicePlugin  parent_instance;

  GHashTable         *transfers;
  ValentTransfer     *upload;
  ValentTransfer     *download;
};

G_DEFINE_FINAL_TYPE (ValentSharePlugin, valent_share_plugin, VALENT_TYPE_DEVICE_PLUGIN)


static GFile *
valent_share_plugin_create_download_file (ValentSharePlugin *self,
                                          const char        *filename,
                                          gboolean           unique)
{
  GSettings *settings;
  g_autofree char *download_folder = NULL;

  g_return_val_if_fail (VALENT_IS_SHARE_PLUGIN (self), NULL);
  g_return_val_if_fail (filename != NULL, NULL);

  /* Check for a configured download directory, returning a fallback if
   * necessary, but don't save the fallback as though it were configured. */
  settings = valent_extension_get_settings (VALENT_EXTENSION (self));
  download_folder = g_settings_get_string (settings, "download-folder");

  if (download_folder == NULL || *download_folder == '\0')
    {
      const char *user_download = NULL;

      user_download = valent_get_user_directory (G_USER_DIRECTORY_DOWNLOAD);
      g_set_str (&download_folder, user_download);
    }

  if (g_mkdir_with_parents (download_folder, 0700) == -1)
    {
      int error = errno;

      g_critical ("%s(): creating \"%s\": %s",
                  G_STRFUNC,
                  download_folder,
                  g_strerror (error));
    }

  return valent_get_user_file (download_folder, filename, unique);
}

/*
 * File Downloads
 */
static void
valent_share_download_file_notification (ValentSharePlugin *self,
                                         ValentTransfer    *transfer)
{
  g_autoptr (ValentDevice) device = NULL;
  g_autoptr (GNotification) notification = NULL;
  g_autoptr (GIcon) icon = NULL;
  const char *title = NULL;
  g_autofree char *body = NULL;
  g_autofree char *id = NULL;
  unsigned int n_files = 0;
  const char *device_name;
  ValentTransferState state = VALENT_TRANSFER_STATE_PENDING;

  g_return_if_fail (VALENT_IS_TRANSFER (transfer));
  g_return_if_fail (VALENT_IS_SHARE_DOWNLOAD (transfer));

  if ((n_files = g_list_model_get_n_items (G_LIST_MODEL (transfer))) == 0)
    return;

  g_object_get (transfer,
                "device", &device,
                "id",     &id,
                "state",  &state,
                NULL);
  device_name = valent_device_get_name (device);

  switch (state)
    {
    case VALENT_TRANSFER_STATE_PENDING:
    case VALENT_TRANSFER_STATE_ACTIVE:
      icon = g_themed_icon_new ("document-save-symbolic");
      title = _("Transferring Files");
      body = g_strdup_printf (ngettext ("Receiving one file from %1$s",
                                        "Receiving %2$d files from %1$s",
                                        n_files),
                              device_name, n_files);
      break;

    case VALENT_TRANSFER_STATE_COMPLETE:
      icon = g_themed_icon_new ("document-save-symbolic");
      title = _("Transfer Complete");
      body = g_strdup_printf (ngettext ("Received one file from %1$s",
                                        "Received %2$d files from %1$s",
                                        n_files),
                              device_name, n_files);
      break;

    case VALENT_TRANSFER_STATE_FAILED:
      icon = g_themed_icon_new ("dialog-warning-symbolic");
      title = _("Transfer Failed");
      body = g_strdup_printf (ngettext ("Receiving one file from %1$s",
                                        "Receiving %2$d files from %1$s",
                                        n_files),
                              device_name, n_files);
      break;
    }

  notification = g_notification_new (title);
  g_notification_set_body (notification, body);
  g_notification_set_icon (notification, icon);

  if (state == VALENT_TRANSFER_STATE_ACTIVE)
    {
      valent_notification_add_device_button (notification,
                                             device,
                                             _("Cancel"),
                                             "share.cancel",
                                             g_variant_new_string (id));
    }
  else if (state == VALENT_TRANSFER_STATE_COMPLETE)
    {
      g_autoptr (ValentTransfer) item = NULL;
      g_autoptr (GFile) file = NULL;
      g_autoptr (GFile) dir = NULL;
      g_autofree char *dirname = NULL;

      item = g_list_model_get_item (G_LIST_MODEL (transfer), 0);
      file = valent_device_transfer_ref_file (VALENT_DEVICE_TRANSFER (item));
      dir = g_file_get_parent (file);
      dirname = g_file_get_uri (dir);

      valent_notification_add_device_button (notification,
                                             device,
                                             _("Open Folder"),
                                             "share.view",
                                             g_variant_new_string (dirname));

      if (n_files == 1)
        {
          g_autofree char *uri = NULL;

          uri = g_file_get_uri (file);
          valent_notification_add_device_button (notification,
                                                 device,
                                                 _("Open File"),
                                                 "share.view",
                                                 g_variant_new_string (uri));
        }
    }

  valent_device_plugin_show_notification (VALENT_DEVICE_PLUGIN (self),
                                          id,
                                          notification);
}

static void
valent_share_download_file_cb (ValentTransfer *transfer,
                               GAsyncResult   *result,
                               gpointer        user_data)
{
  g_autoptr (ValentSharePlugin) self = VALENT_SHARE_PLUGIN (user_data);
  g_autofree char *id = NULL;
  g_autoptr (GError) error = NULL;

  g_assert (VALENT_IS_TRANSFER (transfer));

  id = valent_transfer_dup_id (transfer);

  if (valent_transfer_execute_finish (transfer, result, &error) ||
      !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    valent_share_download_file_notification (self, transfer);
  else
    valent_device_plugin_hide_notification (VALENT_DEVICE_PLUGIN (self), id);

  if (self->download == transfer)
    g_clear_object (&self->download);

  g_hash_table_remove (self->transfers, id);
}

/*
 * File Download (Open)
 */
static void
valent_share_download_open_notification (ValentSharePlugin *self,
                                         ValentTransfer    *transfer)
{
  g_autoptr (ValentDevice) device = NULL;
  g_autoptr (GFile) file = NULL;
  g_autoptr (GNotification) notification = NULL;
  g_autoptr (GIcon) icon = NULL;
  const char *title = NULL;
  g_autofree char *body = NULL;
  g_autofree char *id = NULL;
  g_autofree char *filename = NULL;
  const char *device_name;
  ValentTransferState state = VALENT_TRANSFER_STATE_PENDING;

  g_return_if_fail (VALENT_IS_TRANSFER (transfer));
  g_return_if_fail (VALENT_IS_DEVICE_TRANSFER (transfer));

  g_object_get (transfer,
                "device", &device,
                "file",   &file,
                "id",     &id,
                "state",  &state,
                NULL);
  device_name = valent_device_get_name (device);
  filename = g_file_get_basename (file);

  switch (state)
    {
    case VALENT_TRANSFER_STATE_PENDING:
    case VALENT_TRANSFER_STATE_ACTIVE:
      icon = g_themed_icon_new ("document-save-symbolic");
      title = _("Transferring File");
      body = g_strdup_printf (_("Opening “%s” from “%s”"), filename, device_name);
      break;

    case VALENT_TRANSFER_STATE_COMPLETE:
      valent_device_plugin_hide_notification (VALENT_DEVICE_PLUGIN (self), id);
      return;

    case VALENT_TRANSFER_STATE_FAILED:
      icon = g_themed_icon_new ("dialog-warning-symbolic");
      title = _("Transfer Failed");
      body = g_strdup_printf (_("Opening “%s” from “%s”"), filename, device_name);
      break;
    }

  notification = g_notification_new (title);
  g_notification_set_body (notification, body);
  g_notification_set_icon (notification, icon);

  if (state == VALENT_TRANSFER_STATE_ACTIVE)
    {
      valent_notification_add_device_button (notification,
                                             device,
                                             _("Cancel"),
                                             "share.cancel",
                                             g_variant_new_string (id));
    }

  valent_device_plugin_show_notification (VALENT_DEVICE_PLUGIN (self),
                                          id,
                                          notification);
}

static void
valent_share_download_open_cb (ValentTransfer *transfer,
                               GAsyncResult   *result,
                               gpointer        user_data)
{
  g_autoptr (ValentSharePlugin) self = VALENT_SHARE_PLUGIN (user_data);
  g_autofree char *id = NULL;
  g_autoptr (GError) error = NULL;

  g_assert (VALENT_IS_TRANSFER (transfer));

  id = valent_transfer_dup_id (transfer);

  if (valent_transfer_execute_finish (transfer, result, &error))
    {
      g_autoptr (GFile) file = NULL;
      g_autofree char *uri = NULL;

      file = valent_device_transfer_ref_file (VALENT_DEVICE_TRANSFER (transfer));
      uri = g_file_get_uri (file);

      g_app_info_launch_default_for_uri_async (uri, NULL, NULL, NULL, NULL);
      valent_device_plugin_hide_notification (VALENT_DEVICE_PLUGIN (self), id);
    }
  else if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
      valent_share_download_open_notification (self, transfer);
    }

  g_hash_table_remove (self->transfers, id);
}

/*
 * File Upload (Open)
 */
static void
valent_share_upload_open_notification (ValentSharePlugin *self,
                                       ValentTransfer    *transfer)
{
  g_autoptr (ValentDevice) device = NULL;
  g_autoptr (GFile) file = NULL;
  g_autoptr (GNotification) notification = NULL;
  g_autoptr (GIcon) icon = NULL;
  const char *title = NULL;
  g_autofree char *body = NULL;
  g_autofree char *id = NULL;
  g_autofree char *filename = NULL;
  const char *device_name;
  ValentTransferState state = VALENT_TRANSFER_STATE_PENDING;

  g_return_if_fail (VALENT_IS_TRANSFER (transfer));
  g_return_if_fail (VALENT_IS_DEVICE_TRANSFER (transfer));

  g_object_get (transfer,
                "device", &device,
                "file",   &file,
                "id",     &id,
                "state",  &state,
                NULL);
  device_name = valent_device_get_name (device);
  filename = g_file_get_basename (file);

  switch (state)
    {
    case VALENT_TRANSFER_STATE_PENDING:
    case VALENT_TRANSFER_STATE_ACTIVE:
      icon = g_themed_icon_new ("document-send-symbolic");
      title = _("Transferring File");
      body = g_strdup_printf (_("Opening “%s” on “%s”"), filename, device_name);
      break;

    case VALENT_TRANSFER_STATE_COMPLETE:
      icon = g_themed_icon_new ("document-send-symbolic");
      title = _("Transfer Complete");
      body = g_strdup_printf (_("Opened “%s” on “%s”"), filename, device_name);
      break;

    case VALENT_TRANSFER_STATE_FAILED:
      icon = g_themed_icon_new ("dialog-warning-symbolic");
      title = _("Transfer Failed");
      body = g_strdup_printf (_("Opening “%s” on “%s”"), filename, device_name);
      break;
    }

  notification = g_notification_new (title);
  g_notification_set_body (notification, body);
  g_notification_set_icon (notification, icon);

  if (state == VALENT_TRANSFER_STATE_ACTIVE)
    {
      valent_notification_add_device_button (notification,
                                             device,
                                             _("Cancel"),
                                             "share.cancel",
                                             g_variant_new_string (id));
    }

  valent_device_plugin_show_notification (VALENT_DEVICE_PLUGIN (self),
                                          id,
                                          notification);
}

static void
valent_share_upload_open_cb (ValentTransfer *transfer,
                             GAsyncResult   *result,
                             gpointer        user_data)
{
  g_autoptr (ValentSharePlugin) self = VALENT_SHARE_PLUGIN (user_data);
  g_autofree char *id = NULL;
  g_autoptr (GError) error = NULL;

  g_assert (VALENT_IS_DEVICE_TRANSFER (transfer));
  g_assert (VALENT_IS_SHARE_PLUGIN (self));

  id = valent_transfer_dup_id (transfer);

  if (valent_transfer_execute_finish (transfer, result, &error) ||
      !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    valent_share_upload_open_notification (self, transfer);
  else
    valent_device_plugin_hide_notification (VALENT_DEVICE_PLUGIN (self), id);

  g_hash_table_remove (self->transfers, id);
}

static void
valent_share_plugin_open_file (ValentSharePlugin *self,
                               GFile             *file)
{
  ValentDevice *device = NULL;
  g_autoptr (ValentTransfer) transfer = NULL;
  g_autofree char *filename = NULL;
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonNode) packet = NULL;

  filename = g_file_get_basename (file);

  valent_packet_init (&builder, "kdeconnect.share.request");
  json_builder_set_member_name (builder, "filename");
  json_builder_add_string_value (builder, filename);
  json_builder_set_member_name (builder, "open");
  json_builder_add_boolean_value (builder, TRUE);
  packet = valent_packet_end (&builder);

  /* File uploads that request to be opened are sent as discrete transfers
   * because the remote client (i.e. kdeconnect-android) may download them
   * discretely. Otherwise the remote client may get confused by the
   * `numberOfFiles` field and consider a concurrent multi-file transfer as
   * incomplete.
   */
  device = valent_resource_get_source (VALENT_RESOURCE (self));
  transfer = valent_device_transfer_new (device, packet, file);
  g_hash_table_insert (self->transfers,
                       valent_transfer_dup_id (transfer),
                       g_object_ref (transfer));

  valent_transfer_execute (transfer,
                           NULL,
                           (GAsyncReadyCallback)valent_share_upload_open_cb,
                           g_object_ref (self));
  valent_share_upload_open_notification (self, transfer);
}

/*
 * File Uploads
 */
static void
valent_share_upload_file_notification (ValentSharePlugin *self,
                                       ValentTransfer    *transfer)
{
  g_autoptr (ValentDevice) device = NULL;
  g_autoptr (GNotification) notification = NULL;
  g_autoptr (GIcon) icon = NULL;
  const char *title = NULL;
  g_autofree char *body = NULL;
  g_autofree char *id = NULL;
  unsigned int n_files = 0;
  const char *device_name;
  ValentTransferState state = VALENT_TRANSFER_STATE_PENDING;

  g_return_if_fail (VALENT_IS_TRANSFER (transfer));
  g_return_if_fail (VALENT_IS_SHARE_UPLOAD (transfer));

  if ((n_files = g_list_model_get_n_items (G_LIST_MODEL (transfer))) == 0)
    return;

  g_object_get (transfer,
                "device", &device,
                "id",     &id,
                "state",  &state,
                NULL);
  device_name = valent_device_get_name (device);

  switch (state)
    {
    case VALENT_TRANSFER_STATE_PENDING:
    case VALENT_TRANSFER_STATE_ACTIVE:
      icon = g_themed_icon_new ("document-send-symbolic");
      title = _("Transferring Files");
      body = g_strdup_printf (ngettext ("Sending one file to %1$s",
                                        "Sending %2$d files to %1$s",
                                        n_files),
                              device_name, n_files);
      break;

    case VALENT_TRANSFER_STATE_COMPLETE:
      icon = g_themed_icon_new ("document-send-symbolic");
      title = _("Transfer Complete");
      body = g_strdup_printf (ngettext ("Sent one file to %1$s",
                                        "Sent %2$d files to %1$s",
                                        n_files),
                              device_name, n_files);
      break;

    case VALENT_TRANSFER_STATE_FAILED:
      icon = g_themed_icon_new ("dialog-warning-symbolic");
      title = _("Transfer Failed");
      body = g_strdup_printf (ngettext ("Sending one file to %1$s",
                                        "Sending %2$d files to %1$s",
                                        n_files),
                              device_name, n_files);
      break;
    }

  notification = g_notification_new (title);
  g_notification_set_body (notification, body);
  g_notification_set_icon (notification, icon);

  if (state == VALENT_TRANSFER_STATE_ACTIVE)
    {
      valent_notification_add_device_button (notification,
                                             device,
                                             _("Cancel"),
                                             "share.cancel",
                                             g_variant_new_string (id));
    }

  valent_device_plugin_show_notification (VALENT_DEVICE_PLUGIN (self),
                                          id,
                                          notification);
}

static void
valent_share_upload_file_cb (ValentTransfer *transfer,
                             GAsyncResult   *result,
                             gpointer        user_data)
{
  g_autoptr (ValentSharePlugin) self = VALENT_SHARE_PLUGIN (user_data);
  g_autofree char *id = NULL;
  g_autoptr (GError) error = NULL;

  g_assert (VALENT_IS_SHARE_UPLOAD (transfer));
  g_assert (VALENT_IS_SHARE_PLUGIN (self));

  id = valent_transfer_dup_id (transfer);

  if (valent_transfer_execute_finish (transfer, result, &error) ||
      !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    valent_share_upload_file_notification (self, transfer);
  else
    valent_device_plugin_hide_notification (VALENT_DEVICE_PLUGIN (self), id);

  if (self->upload == transfer)
    g_clear_object (&self->upload);

  g_hash_table_remove (self->transfers, id);
}

static void
valent_share_upload_files_added (ValentTransfer    *transfer,
                                 unsigned int       position,
                                 unsigned int       removed,
                                 unsigned int       added,
                                 ValentSharePlugin *self)
{
  g_assert (VALENT_IS_SHARE_UPLOAD (transfer));
  g_assert (VALENT_IS_SHARE_PLUGIN (self));

  /* If no files were added, something went wrong */
  g_return_if_fail (added > 0);

  /* Start the transfer, if necessary */
  if (valent_transfer_get_state (transfer) == VALENT_TRANSFER_STATE_PENDING)
    {
      valent_transfer_execute (transfer,
                               NULL,
                               (GAsyncReadyCallback)valent_share_upload_file_cb,
                               g_object_ref (self));
    }

  valent_share_upload_file_notification (self, transfer);
}

static void
valent_share_plugin_upload_file (ValentSharePlugin *self,
                                 GFile             *file)
{
  g_assert (VALENT_IS_SHARE_PLUGIN (self));
  g_assert (G_IS_FILE (file));

  /* Create a new transfer, if necessary */
  if (self->upload == NULL)
    {
      ValentDevice *device;

      device = valent_resource_get_source (VALENT_RESOURCE (self));

      self->upload = valent_share_upload_new (device);
      g_signal_connect_object (self->upload,
                               "items-changed",
                               G_CALLBACK (valent_share_upload_files_added),
                               self, 0);
      g_hash_table_replace (self->transfers,
                            valent_transfer_dup_id (self->upload),
                            g_object_ref (self->upload));
    }

  valent_share_upload_add_file (VALENT_SHARE_UPLOAD (self->upload), file);
}

/*
 * GActions
 */
/**
 * ValentSharePlugin|share.cancel:
 * @parameter: "s"
 * @id: The transfer ID
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
  ValentTransfer *transfer;
  const char *id;

  g_assert (VALENT_IS_SHARE_PLUGIN (self));

  id = g_variant_get_string (parameter, NULL);
  transfer = g_hash_table_lookup (self->transfers, id);

  if (transfer != NULL)
    valent_transfer_cancel (transfer);
}

/**
 * ValentSharePlugin|share.copy:
 * @parameter: "s"
 * @text: The text content
 *
 * This action allows copying shared text to the clipboard from a notification.
 */
static void
share_copy_action (GSimpleAction *action,
                   GVariant      *parameter,
                   gpointer       user_data)
{
  const char *text;

  g_assert (VALENT_IS_SHARE_PLUGIN (user_data));

  text = g_variant_get_string (parameter, NULL);
  valent_clipboard_write_text (valent_clipboard_get_default (),
                               text,
                               NULL,
                               NULL,
                               NULL);
}

/**
 * ValentSharePlugin|share.open:
 * @parameter: "s"
 * @uri: File URI to open
 *
 * This action is used to open a URI.
 *
 * By convention, the remote device will open the URI with the default handler
 * for that type.
 *
 * If the URI scheme is `file://`, it will be converted to a file upload,
 * requesting it be opened after transfer.
 */
static void
share_open_action (GSimpleAction *action,
                   GVariant      *parameter,
                   gpointer       user_data)
{
  ValentSharePlugin *self = VALENT_SHARE_PLUGIN (user_data);
  const char *uri_string = NULL;
  g_autoptr (GUri) uri = NULL;
  g_autoptr (GError) error = NULL;

  g_assert (VALENT_IS_SHARE_PLUGIN (self));
  g_assert (g_variant_is_of_type (parameter, G_VARIANT_TYPE_STRING));

  uri_string = g_variant_get_string (parameter, NULL);

  if ((uri = g_uri_parse (uri_string, G_URI_FLAGS_NONE, &error)) == NULL)
    {
      g_warning ("%s(): %s", G_STRFUNC, error->message);
      return;
    }

  if (g_str_equal ("file", g_uri_get_scheme (uri)) ||
      g_str_equal ("resource", g_uri_get_scheme (uri)))
    {
      g_autoptr (GFile) file = NULL;

      file = g_file_new_for_uri (uri_string);
      valent_share_plugin_open_file (self, file);
    }
  else
    {
      g_autoptr (JsonBuilder) builder = NULL;
      g_autoptr (JsonNode) packet = NULL;

      valent_packet_init (&builder, "kdeconnect.share.request");
      json_builder_set_member_name (builder, "url");
      json_builder_add_string_value (builder, uri_string);
      packet = valent_packet_end (&builder);

      valent_device_plugin_queue_packet (VALENT_DEVICE_PLUGIN (self), packet);
    }
}

static void
share_save_action_cb (GFile        *file,
                      GAsyncResult *result,
                      gpointer      user_data)
{
  ValentSharePlugin *self = VALENT_SHARE_PLUGIN (user_data);
  g_autoptr (GNotification) notification = NULL;
  g_autoptr (GIcon) icon = NULL;
  g_autoptr (GFile) parent = NULL;
  ValentDevice *device = NULL;
  g_autofree char *title = NULL;
  g_autofree char *dir_uri = NULL;
  g_autofree char *file_uri = NULL;
  g_autofree char *basename = NULL;
  const char *name = NULL;
  g_autoptr (GError) error = NULL;

  g_assert (VALENT_IS_SHARE_PLUGIN (self));

  if (!g_file_replace_contents_finish (file, result, NULL, &error))
    {
      g_warning ("Saving \"%s\": %s", g_file_peek_path (file), error->message);
      return;
    }

  device = valent_resource_get_source (VALENT_RESOURCE (self));
  name = valent_device_get_name (device);
  parent = g_file_get_parent (file);
  dir_uri = g_file_get_uri (parent);
  file_uri = g_file_get_uri (file);
  basename = g_file_get_basename (file);

  title = g_strdup_printf (_("Text from “%s” saved to “%s”"), name, basename);
  icon = g_themed_icon_new ("document-save-symbolic");

  notification = g_notification_new (title);
  g_notification_set_icon (notification, icon);
  valent_notification_add_device_button (notification,
                                         device,
                                         _("Open Folder"),
                                         "share.view",
                                         g_variant_new_string (dir_uri));
  valent_notification_add_device_button (notification,
                                         device,
                                         _("Open File"),
                                         "share.view",
                                         g_variant_new_string (file_uri));
  valent_device_plugin_show_notification (VALENT_DEVICE_PLUGIN (self),
                                          file_uri,
                                          notification);
}

/**
 * ValentSharePlugin|share.save:
 * @parameter: "s"
 * @text: The text content
 *
 * This action allows saving shared text to file from a notification.
 */
static void
share_save_action (GSimpleAction *action,
                   GVariant      *parameter,
                   gpointer       user_data)
{
  ValentSharePlugin *self = VALENT_SHARE_PLUGIN (user_data);
  ValentDevice *device = NULL;
  g_autoptr (GBytes) bytes = NULL;
  g_autoptr (GDateTime) date = NULL;
  g_autofree char *date_str = NULL;
  g_autoptr (GFile) file = NULL;
  g_autofree char *filename = NULL;
  const char *name;
  const char *text;

  g_assert (VALENT_IS_SHARE_PLUGIN (self));

  device = valent_resource_get_source (VALENT_RESOURCE (self));
  name = valent_device_get_name (device);
  text = g_variant_get_string (parameter, NULL);

  bytes = g_bytes_new (text, strlen (text));
  date = g_date_time_new_now_local ();
  date_str = g_date_time_format (date, "%F %T");
  /* TRANSLATORS: this is a filename used for text shared by a device, where
   * the first "%s" is the date and the second "%s" is the device name, e.g.
   * "Text from 07-12-2024 10:00:46 PM (OnePlus 6)"
   * */
  filename = g_strdup_printf (_("Text from %s (%s).txt"), date_str, name);
  file = valent_share_plugin_create_download_file (self, filename, TRUE);

  g_file_replace_contents_bytes_async (file,
                                       bytes,
                                       NULL,
                                       FALSE,
                                       G_FILE_CREATE_REPLACE_DESTINATION,
                                       NULL,
                                       (GAsyncReadyCallback)share_save_action_cb,
                                       NULL);
}

/**
 * ValentSharePlugin|share.text:
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
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonNode) packet = NULL;

  g_assert (VALENT_IS_SHARE_PLUGIN (self));
  g_assert (g_variant_is_of_type (parameter, G_VARIANT_TYPE_STRING));

  text = g_variant_get_string (parameter, NULL);

  valent_packet_init (&builder, "kdeconnect.share.request");
  json_builder_set_member_name (builder, "text");
  json_builder_add_string_value (builder, text);
  packet = valent_packet_end (&builder);

  valent_device_plugin_queue_packet (VALENT_DEVICE_PLUGIN (self), packet);
}

/**
 * ValentSharePlugin|share.uri:
 * @parameter: "s"
 * @uri: URI to share
 *
 * This action is used to share a URI.
 *
 * By convention, the remote device will open the URI with the default handler
 * for that type.
 *
 * If the URI scheme is `file://`, it will be converted to a file upload.
 */
static void
share_uri_action (GSimpleAction *action,
                  GVariant      *parameter,
                  gpointer       user_data)
{
  ValentSharePlugin *self = VALENT_SHARE_PLUGIN (user_data);
  const char *uri_string = NULL;
  g_autoptr (GUri) uri = NULL;
  g_autoptr (GError) error = NULL;

  g_assert (VALENT_IS_SHARE_PLUGIN (self));
  g_assert (g_variant_is_of_type (parameter, G_VARIANT_TYPE_STRING));

  uri_string = g_variant_get_string (parameter, NULL);

  if ((uri = g_uri_parse (uri_string, G_URI_FLAGS_NONE, &error)) == NULL)
    {
      g_warning ("%s(): %s", G_STRFUNC, error->message);
      return;
    }

  if (g_str_equal ("file", g_uri_get_scheme (uri)) ||
      g_str_equal ("resource", g_uri_get_scheme (uri)))
    {
      g_autoptr (GFile) file = NULL;

      file = g_file_new_for_uri (uri_string);
      valent_share_plugin_upload_file (self, file);
    }
  else
    {
      g_autoptr (JsonBuilder) builder = NULL;
      g_autoptr (JsonNode) packet = NULL;

      valent_packet_init (&builder, "kdeconnect.share.request");
      json_builder_set_member_name (builder, "url");
      json_builder_add_string_value (builder, uri_string);
      packet = valent_packet_end (&builder);

      valent_device_plugin_queue_packet (VALENT_DEVICE_PLUGIN (self), packet);
    }
}

/**
 * ValentSharePlugin|share.uris:
 * @parameter: "as"
 * @uris: a list of URIs
 *
 * This action is a convenience for sending multiple URIs, as with the
 * `ValentSharePlugin|share.uri` `GAction`.
 */
static void
share_uris_action (GSimpleAction *action,
                   GVariant      *parameter,
                   gpointer       user_data)
{
  GVariantIter iter;
  GVariant *child;

  g_assert (VALENT_IS_SHARE_PLUGIN (user_data));
  g_assert (g_variant_is_of_type (parameter, G_VARIANT_TYPE_STRING_ARRAY));

  g_variant_iter_init (&iter, parameter);

  while ((child = g_variant_iter_next_value (&iter)) != NULL)
    {
      share_uri_action (action, child, user_data);
      g_clear_pointer (&child, g_variant_unref);
    }
}

/**
 * ValentSharePlugin|share.view:
 * @parameter: "s"
 * @uri: File or directory URI to view
 *
 * This action opens a file or directory.
 */
static void
share_view_action (GSimpleAction *action,
                   GVariant      *parameter,
                   gpointer       user_data)
{
  const char *uri;

  g_assert (VALENT_IS_SHARE_PLUGIN (user_data));
  g_assert (g_variant_is_of_type (parameter, G_VARIANT_TYPE_STRING));

  uri = g_variant_get_string (parameter, NULL);
  g_app_info_launch_default_for_uri_async (uri, NULL, NULL, NULL, NULL);
}

static GActionEntry actions[] = {
    {"cancel",  share_cancel_action,  "s",  NULL, NULL},
    {"copy",    share_copy_action,    "s",  NULL, NULL},
    {"open",    share_open_action,    "s",  NULL, NULL},
    {"save",    share_save_action,    "s",  NULL, NULL},
    {"text",    share_text_action,    "s",  NULL, NULL},
    {"uri",     share_uri_action,     "s",  NULL, NULL},
    {"uris",    share_uris_action,    "as", NULL, NULL},
    {"view",    share_view_action,    "s",  NULL, NULL},
};

/*
 * Packet Handlers
 */
static void
valent_share_plugin_handle_file (ValentSharePlugin *self,
                                 JsonNode          *packet)
{
  g_autoptr (ValentTransfer) transfer = NULL;
  ValentDevice *device;
  const char *filename;
  g_autoptr (GFile) file = NULL;
  int64_t number_of_files = 0;

  g_assert (VALENT_IS_SHARE_PLUGIN (self));
  g_assert (VALENT_IS_PACKET (packet));

  /* Common packet fields */
  if (!valent_packet_has_payload (packet))
    {
      g_warning ("%s(): missing payload info", G_STRFUNC);
      return;
    }

  if (!valent_packet_get_string (packet, "filename", &filename))
    {
      g_debug ("%s(): expected \"filename\" field holding a string",
               G_STRFUNC);
      return;
    }

  /* Newer implementations support sequential multi-file transfers */
  if (!valent_packet_get_int (packet, "numberOfFiles", &number_of_files))
    {
      json_object_set_int_member (valent_packet_get_body (packet),
                                  "numberOfFiles",
                                  1);
    }

  if (!valent_packet_get_int (packet, "totalPayloadSize", NULL))
    {
      json_object_set_int_member (valent_packet_get_body (packet),
                                  "totalPayloadSize",
                                  valent_packet_get_payload_size (packet));
    }

  file = valent_share_plugin_create_download_file (self, filename, TRUE);
  device = valent_resource_get_source (VALENT_RESOURCE (self));

  /* If the packet includes a request to open the file when the transfer
   * completes, use a separate routine for success/failure. */
  if (valent_packet_check_field (packet, "open"))
    {
      transfer = valent_device_transfer_new (device, packet, file);
      g_hash_table_replace (self->transfers,
                            valent_transfer_dup_id (transfer),
                            g_object_ref (transfer));

      valent_transfer_execute (transfer,
                               NULL,
                               (GAsyncReadyCallback)valent_share_download_open_cb,
                               g_object_ref (self));
      valent_share_download_open_notification (self, transfer);
      return;
    }

  /* If the packet is missing the `numberOfFiles` field it is a legacy transfer
   * transfer; use a discrete transfer with standard success/failure handling. */
  if (!number_of_files)
    {
      transfer = valent_share_download_new (device);
      g_hash_table_replace (self->transfers,
                            valent_transfer_dup_id (transfer),
                            g_object_ref (transfer));

      valent_share_download_add_file (VALENT_SHARE_DOWNLOAD (transfer),
                                      file,
                                      packet);

      valent_transfer_execute (transfer,
                               NULL,
                               (GAsyncReadyCallback)valent_share_download_file_cb,
                               g_object_ref (self));
      valent_share_download_file_notification (self, transfer);
      return;
    }

  /* Otherwise the file will appended to a multi-file transfer */
  if (self->download != NULL)
    {
      transfer = g_object_ref (self->download);
    }
  else
    {
      transfer = valent_share_download_new (device);
      g_hash_table_replace (self->transfers,
                            valent_transfer_dup_id (transfer),
                            g_object_ref (transfer));
    }

  valent_share_download_add_file (VALENT_SHARE_DOWNLOAD (transfer),
                                  file,
                                  packet);

  if (self->download != transfer)
    g_set_object (&self->download, transfer);

  if (valent_transfer_get_state (transfer) == VALENT_TRANSFER_STATE_PENDING)
    {
      valent_transfer_execute (transfer,
                               NULL,
                               (GAsyncReadyCallback)valent_share_download_file_cb,
                               g_object_ref (self));
    }

  valent_share_download_file_notification (self, transfer);
}

static void
valent_share_plugin_handle_file_update (ValentSharePlugin *self,
                                        JsonNode          *packet)
{
  g_assert (VALENT_IS_SHARE_PLUGIN (self));
  g_assert (VALENT_IS_PACKET (packet));

  if (self->download == NULL)
    return;

  if (!valent_packet_check_field (packet, "numberOfFiles"))
    {
      g_debug ("%s(): expected \"numberOfFiles\" field holding an integer",
               G_STRFUNC);
      return;
    }

  if (!valent_packet_check_field (packet, "totalPayloadSize"))
    {
      g_debug ("%s(): expected \"totalPayloadSize\" field holding an integer",
               G_STRFUNC);
      return;
    }

  valent_share_download_update (VALENT_SHARE_DOWNLOAD (self->download), packet);
  valent_share_download_file_notification (self, self->download);
}

static void
valent_share_plugin_handle_text (ValentSharePlugin *self,
                                 const char        *text)
{
  ValentResource *resource = VALENT_RESOURCE (self);
  ValentDevice *device = NULL;
  g_autoptr (GNotification) notification = NULL;
  g_autofree char *id = NULL;
  g_autofree char *title = NULL;
  const char *name = NULL;

  g_assert (VALENT_IS_SHARE_PLUGIN (self));
  g_assert (text != NULL);

  device = valent_resource_get_source (resource);
  name = valent_device_get_name (device);
  id = g_compute_checksum_for_string (G_CHECKSUM_MD5, text, -1);
  title = g_strdup_printf (_("Text from “%s”"), name);

  notification = g_notification_new (title);
  g_notification_set_body (notification, text);
  valent_notification_add_device_button (notification,
                                         device,
                                         _("Save"),
                                         "share.save",
                                         g_variant_new_string (text));
  valent_notification_add_device_button (notification,
                                         device,
                                         _("Copy"),
                                         "share.copy",
                                         g_variant_new_string (text));

  valent_device_plugin_show_notification (VALENT_DEVICE_PLUGIN (self),
                                          id,
                                          notification);
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
valent_share_plugin_update_state (ValentDevicePlugin *plugin,
                                  ValentDeviceState   state)
{
  ValentSharePlugin *self = VALENT_SHARE_PLUGIN (plugin);
  gboolean available;

  g_assert (VALENT_IS_SHARE_PLUGIN (plugin));

  available = (state & VALENT_DEVICE_STATE_CONNECTED) != 0 &&
              (state & VALENT_DEVICE_STATE_PAIRED) != 0;

  /* If the device has been unpaired it should be considered untrusted, so
   * cancel any ongoing transfers. */
  if ((state & VALENT_DEVICE_STATE_PAIRED) == 0)
    {
      GHashTableIter iter;
      ValentTransfer *transfer;

      g_hash_table_iter_init (&iter, self->transfers);

      while (g_hash_table_iter_next (&iter, NULL, (void **)&transfer))
        {
          valent_transfer_cancel (transfer);
          g_hash_table_iter_remove (&iter);
        }

      g_clear_object (&self->download);
      g_clear_object (&self->upload);
    }

  valent_extension_toggle_actions (VALENT_EXTENSION (plugin), available);
}

static void
valent_share_plugin_handle_packet (ValentDevicePlugin *plugin,
                                   const char         *type,
                                   JsonNode           *packet)
{
  ValentSharePlugin *self = VALENT_SHARE_PLUGIN (plugin);
  const char *text;
  const char *url;

  g_assert (VALENT_IS_SHARE_PLUGIN (self));
  g_assert (type != NULL);
  g_assert (VALENT_IS_PACKET (packet));

  if (g_str_equal (type, "kdeconnect.share.request"))
    {
      if (valent_packet_check_field (packet, "filename"))
        valent_share_plugin_handle_file (self, packet);

      else if (valent_packet_get_string (packet, "text", &text))
        valent_share_plugin_handle_text (self, text);

      else if (valent_packet_get_string (packet, "url", &url))
        valent_share_plugin_handle_url (self, url);

      else
        g_warning ("%s(): unsupported share request", G_STRFUNC);
    }
  else if (g_str_equal (type, "kdeconnect.share.request.update"))
    {
      valent_share_plugin_handle_file_update (self, packet);
    }
  else
    g_assert_not_reached ();
}

/*
 * ValentObject
 */
static void
valent_share_plugin_destroy (ValentObject *object)
{
  ValentSharePlugin *self = VALENT_SHARE_PLUGIN (object);
  GHashTableIter iter;
  ValentTransfer *transfer;

  /* Cancel active transfers */
  g_hash_table_iter_init (&iter, self->transfers);

  while (g_hash_table_iter_next (&iter, NULL, (void **)&transfer))
    {
      valent_transfer_cancel (transfer);
      g_hash_table_iter_remove (&iter);
    }

  g_clear_object (&self->download);
  g_clear_object (&self->upload);

  VALENT_OBJECT_CLASS (valent_share_plugin_parent_class)->destroy (object);
}

/*
 * GObject
 */
static void
valent_share_plugin_constructed (GObject *object)
{
  ValentDevicePlugin *plugin = VALENT_DEVICE_PLUGIN (object);

  G_OBJECT_CLASS (valent_share_plugin_parent_class)->constructed (object);

  g_action_map_add_action_entries (G_ACTION_MAP (plugin),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   plugin);
}

static void
valent_share_plugin_finalize (GObject *object)
{
  ValentSharePlugin *self = VALENT_SHARE_PLUGIN (object);

  g_clear_pointer (&self->transfers, g_hash_table_unref);

  G_OBJECT_CLASS (valent_share_plugin_parent_class)->finalize (object);
}

static void
valent_share_plugin_class_init (ValentSharePluginClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentObjectClass *vobject_class = VALENT_OBJECT_CLASS (klass);
  ValentDevicePluginClass *plugin_class = VALENT_DEVICE_PLUGIN_CLASS (klass);

  object_class->constructed = valent_share_plugin_constructed;
  object_class->finalize = valent_share_plugin_finalize;

  vobject_class->destroy = valent_share_plugin_destroy;

  plugin_class->handle_packet = valent_share_plugin_handle_packet;
  plugin_class->update_state = valent_share_plugin_update_state;
}

static void
valent_share_plugin_init (ValentSharePlugin *self)
{
  self->transfers = g_hash_table_new_full (g_str_hash,
                                           g_str_equal,
                                           g_free,
                                           g_object_unref);
}

