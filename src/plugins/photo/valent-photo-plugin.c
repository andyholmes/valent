// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-photo-plugin"

#include "config.h"

#include <glib/gi18n.h>
#include <gio/gio.h>
#include <json-glib/json-glib.h>
#include <valent.h>

#include "valent-photo-plugin.h"


struct _ValentPhotoPlugin
{
  ValentDevicePlugin  parent_instance;
};

G_DEFINE_FINAL_TYPE (ValentPhotoPlugin, valent_photo_plugin, VALENT_TYPE_DEVICE_PLUGIN)


/*
 * Remote Camera
 */
static void
valent_transfer_execute_cb (ValentTransfer     *transfer,
                            GAsyncResult       *result,
                            ValentDevicePlugin *plugin)
{
  g_autoptr (GFile) file = NULL;
  g_autoptr (GError) error = NULL;

  g_assert (VALENT_IS_DEVICE_TRANSFER (transfer));

  g_object_get (transfer, "file", &file, NULL);

  if (valent_transfer_execute_finish (transfer, result, &error))
    {
      VALENT_TODO ("GSetting to open on completion");
    }
  else if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
      g_autoptr (GNotification) notification = NULL;
      g_autoptr (GIcon) icon = NULL;
      g_autofree char *body = NULL;
      g_autofree char *filename = NULL;
      ValentDevice *device;

      device = valent_device_plugin_get_device (plugin);
      filename = g_file_get_basename (file);
      icon = g_themed_icon_new ("dialog-error-symbolic");
      body = g_strdup_printf (_("Failed to receive “%s” from %s"),
                              filename,
                              valent_device_get_name (device));

      notification = g_notification_new (_("Transfer Failed"));
      g_notification_set_body (notification, body);
      g_notification_set_icon (notification, icon);
      valent_device_plugin_show_notification (plugin, "photo", notification);
    }
}

static void
valent_photo_plugin_handle_photo (ValentPhotoPlugin *self,
                                  JsonNode          *packet)
{
  g_autoptr (ValentTransfer) transfer = NULL;
  g_autoptr (GCancellable) cancellable = NULL;
  g_autoptr (GFile) file = NULL;
  const char *directory = NULL;
  ValentDevice *device;
  const char *filename;

  g_assert (VALENT_IS_PHOTO_PLUGIN (self));
  g_assert (VALENT_IS_PACKET (packet));

  if (!valent_packet_has_payload (packet))
    {
      g_warning ("%s(): missing payload info", G_STRFUNC);
      return;
    }

  if (!valent_packet_get_string (packet, "filename", &filename))
    {
      g_warning ("%s(): expected \"filename\" field holding a string",
                 G_STRFUNC);
      return;
    }

  device = valent_device_plugin_get_device (VALENT_DEVICE_PLUGIN (self));
  cancellable = valent_object_ref_cancellable (VALENT_OBJECT (self));
  directory = valent_get_user_directory (G_USER_DIRECTORY_PICTURES);
  file = valent_get_user_file (directory, filename, TRUE);

  /* Create a new transfer */
  transfer = valent_device_transfer_new_for_file (device, packet, file);
  valent_transfer_execute (transfer,
                           cancellable,
                           (GAsyncReadyCallback)valent_transfer_execute_cb,
                           self);
}

static void
valent_photo_plugin_handle_photo_request (ValentPhotoPlugin *self,
                                          JsonNode          *packet)
{
  g_assert (VALENT_IS_PHOTO_PLUGIN (self));
  g_assert (VALENT_IS_PACKET (packet));

  VALENT_TODO ("A request for a photo");
}

/*
 * GActions
 */
static void
photo_request_action (GSimpleAction *action,
                      GVariant      *parameter,
                      gpointer       user_data)
{
  ValentPhotoPlugin *self = VALENT_PHOTO_PLUGIN (user_data);
  g_autoptr (JsonNode) packet = NULL;

  g_return_if_fail (VALENT_IS_PHOTO_PLUGIN (self));

  packet = valent_packet_new ("kdeconnect.photo.request");
  valent_device_plugin_queue_packet (VALENT_DEVICE_PLUGIN (self), packet);
}

static const GActionEntry actions[] = {
    {"request", photo_request_action, NULL, NULL, NULL}
};

static const ValentMenuEntry items[] = {
    {N_("Take Photo"), "device.photo.request", "camera-photo-symbolic"}
};

/*
 * ValentDevicePlugin
 */
static void
valent_photo_plugin_enable (ValentDevicePlugin *plugin)
{
  g_assert (VALENT_IS_PHOTO_PLUGIN (plugin));

  g_action_map_add_action_entries (G_ACTION_MAP (plugin),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   plugin);
  valent_device_plugin_add_menu_entries (plugin,
                                         items,
                                         G_N_ELEMENTS (items));
}

static void
valent_photo_plugin_disable (ValentDevicePlugin *plugin)
{
  g_assert (VALENT_IS_PHOTO_PLUGIN (plugin));

  valent_device_plugin_remove_menu_entries (plugin,
                                            items,
                                            G_N_ELEMENTS (items));
}

static void
valent_photo_plugin_update_state (ValentDevicePlugin *plugin,
                                  ValentDeviceState   state)
{
  gboolean available;

  g_assert (VALENT_IS_PHOTO_PLUGIN (plugin));

  available = (state & VALENT_DEVICE_STATE_CONNECTED) != 0 &&
              (state & VALENT_DEVICE_STATE_PAIRED) != 0;

  valent_device_plugin_toggle_actions (plugin, available);
}

static void
valent_photo_plugin_handle_packet (ValentDevicePlugin *plugin,
                                   const char         *type,
                                   JsonNode           *packet)
{
  ValentPhotoPlugin *self = VALENT_PHOTO_PLUGIN (plugin);

  g_assert (VALENT_IS_PHOTO_PLUGIN (plugin));
  g_assert (type != NULL);
  g_assert (VALENT_IS_PACKET (packet));

  if (strcmp (type, "kdeconnect.photo") == 0)
    valent_photo_plugin_handle_photo (self, packet);

  else if (strcmp (type, "kdeconnect.photo.request") == 0)
    valent_photo_plugin_handle_photo_request (self, packet);

  else
    g_assert_not_reached ();
}

/*
 * GObject
 */
static void
valent_photo_plugin_class_init (ValentPhotoPluginClass *klass)
{
  ValentDevicePluginClass *plugin_class = VALENT_DEVICE_PLUGIN_CLASS (klass);

  plugin_class->enable = valent_photo_plugin_enable;
  plugin_class->disable = valent_photo_plugin_disable;
  plugin_class->handle_packet = valent_photo_plugin_handle_packet;
  plugin_class->update_state = valent_photo_plugin_update_state;
}

static void
valent_photo_plugin_init (ValentPhotoPlugin *self)
{
}

