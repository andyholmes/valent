// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-photo-plugin"

#include "config.h"

#include <glib/gi18n.h>
#include <libpeas/peas.h>
#include <libvalent-core.h>

#include "valent-photo-plugin.h"


struct _ValentPhotoPlugin
{
  ValentDevicePlugin  parent_instance;
};

G_DEFINE_TYPE (ValentPhotoPlugin, valent_photo_plugin, VALENT_TYPE_DEVICE_PLUGIN)


/**
 * Packet Handlers
 */
typedef struct
{
  ValentPhotoPlugin *plugin;
  GFile             *file;
} DownloadOperation;

static void
transfer_cb (ValentTransfer    *transfer,
             GAsyncResult      *result,
             DownloadOperation *op)
{
  ValentPhotoPlugin *self = op->plugin;
  g_autoptr (GError) error = NULL;

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

      g_warning ("Transfer failed: %s", error->message);

      device = valent_device_plugin_get_device (VALENT_DEVICE_PLUGIN (self));
      filename = g_file_get_basename (op->file);
      icon = g_themed_icon_new ("dialog-error-symbolic");
      body = g_strdup_printf (_("Failed to receive “%s” from %s"),
                              filename,
                              valent_device_get_name (device));

      notification = g_notification_new (_("Transfer Failed"));
      g_notification_set_body (notification, body);
      g_notification_set_icon (notification, icon);

      valent_device_plugin_show_notification (VALENT_DEVICE_PLUGIN (self),
                                              "photo",
                                              notification);
    }

  g_clear_object (&op->plugin);
  g_clear_object (&op->file);
  g_free (op);
}

static void
valent_photo_plugin_handle_photo (ValentPhotoPlugin *self,
                                  JsonNode          *packet)
{
  g_autoptr (ValentTransfer) transfer = NULL;
  ValentDevice *device;
  DownloadOperation *op;
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

  op = g_new0 (DownloadOperation, 1);
  op->plugin = g_object_ref (self);
  op->file = valent_device_new_download_file (device, filename, TRUE);

  /* Create a new transfer */
  transfer = valent_transfer_new (device);
  valent_transfer_add_file (transfer, packet, op->file);
  valent_transfer_execute (transfer,
                           NULL,
                           (GAsyncReadyCallback)transfer_cb,
                           op);
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
photo_action (GSimpleAction *action,
              GVariant      *parameter,
              gpointer       user_data)
{
  ValentPhotoPlugin *self = VALENT_PHOTO_PLUGIN (user_data);
  JsonBuilder *builder;
  g_autoptr (JsonNode) packet = NULL;

  g_return_if_fail (VALENT_IS_PHOTO_PLUGIN (self));

  builder = valent_packet_start ("kdeconnect.photo.request");
  packet = valent_packet_finish (builder);

  valent_device_plugin_queue_packet (VALENT_DEVICE_PLUGIN (self), packet);
}

static const GActionEntry actions[] = {
    {"photo", photo_action, NULL, NULL, NULL}
};

static const ValentMenuEntry items[] = {
    {N_("Take Photo"), "device.photo.photo", "camera-photo-symbolic"}
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

  if (g_strcmp0 (type, "kdeconnect.photo") == 0)
    valent_photo_plugin_handle_photo (self, packet);

  else if (g_strcmp0 (type, "kdeconnect.photo.request") == 0)
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

