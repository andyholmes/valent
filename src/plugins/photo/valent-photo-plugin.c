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
  PeasExtensionBase  parent_instance;

  ValentDevice      *device;
};

static void valent_device_plugin_iface_init (ValentDevicePluginInterface *iface);

G_DEFINE_TYPE_WITH_CODE (ValentPhotoPlugin, valent_photo_plugin, PEAS_TYPE_EXTENSION_BASE,
                         G_IMPLEMENT_INTERFACE (VALENT_TYPE_DEVICE_PLUGIN, valent_device_plugin_iface_init))

enum {
  PROP_0,
  PROP_DEVICE,
  N_PROPERTIES
};


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

      g_warning ("Transfer failed: %s", error->message);

      filename = g_file_get_basename (op->file);
      icon = g_themed_icon_new ("dialog-error-symbolic");
      body = g_strdup_printf (_("Failed to receive “%s” from %s"),
                              filename,
                              valent_device_get_name (self->device));

      notification = g_notification_new (_("Transfer Failed"));
      g_notification_set_body (notification, body);
      g_notification_set_icon (notification, icon);

      valent_device_show_notification (self->device, "photo", notification);
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
  DownloadOperation *op;
  JsonObject *body;
  const char *filename;

  g_assert (VALENT_IS_PHOTO_PLUGIN (self));
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

  op = g_new0 (DownloadOperation, 1);
  op->plugin = g_object_ref (self);
  op->file = valent_device_new_download_file (self->device, filename, TRUE);

  /* Create a new transfer */
  transfer = valent_transfer_new (self->device);
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

  valent_device_queue_packet (self->device, packet);
}

static const GActionEntry actions[] = {
    {"photo", photo_action, NULL, NULL, NULL}
};

static const ValentMenuEntry items[] = {
    {N_("Take Photo"), "device.photo", "camera-photo-symbolic"}
};

/*
 * ValentDevicePlugin
 */
static void
valent_photo_plugin_enable (ValentDevicePlugin *plugin)
{
  ValentPhotoPlugin *self = VALENT_PHOTO_PLUGIN (plugin);

  g_assert (VALENT_IS_PHOTO_PLUGIN (self));

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
valent_photo_plugin_disable (ValentDevicePlugin *plugin)
{
  /* Unregister GMenu items */
  valent_device_plugin_remove_menu_entries (plugin,
                                            items,
                                            G_N_ELEMENTS (items));

  /* Unregister GActions */
  valent_device_plugin_unregister_actions (plugin,
                                           actions,
                                           G_N_ELEMENTS (actions));
}

static void
valent_photo_plugin_update_state (ValentDevicePlugin *plugin)
{
  ValentPhotoPlugin *self = VALENT_PHOTO_PLUGIN (plugin);
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

static void
valent_device_plugin_iface_init (ValentDevicePluginInterface *iface)
{
  iface->enable = valent_photo_plugin_enable;
  iface->disable = valent_photo_plugin_disable;
  iface->handle_packet = valent_photo_plugin_handle_packet;
  iface->update_state = valent_photo_plugin_update_state;
}

/**
 * GObject
 */
static void
valent_photo_plugin_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  ValentPhotoPlugin *self = VALENT_PHOTO_PLUGIN (object);

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
valent_photo_plugin_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  ValentPhotoPlugin *self = VALENT_PHOTO_PLUGIN (object);

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
valent_photo_plugin_class_init (ValentPhotoPluginClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = valent_photo_plugin_get_property;
  object_class->set_property = valent_photo_plugin_set_property;

  g_object_class_override_property (object_class, PROP_DEVICE, "device");
}

static void
valent_photo_plugin_init (ValentPhotoPlugin *self)
{
}

