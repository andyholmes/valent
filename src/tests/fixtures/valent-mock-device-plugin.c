// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <libpeas/peas.h>
#include <libvalent-core.h>

#include "valent-mock-device-plugin.h"


struct _ValentMockDevicePlugin
{
  PeasExtensionBase  parent_instance;

  ValentDevice      *device;
};

static void valent_device_plugin_iface_init (ValentDevicePluginInterface *iface);

G_DEFINE_TYPE_WITH_CODE (ValentMockDevicePlugin, valent_mock_device_plugin, PEAS_TYPE_EXTENSION_BASE,
                         G_IMPLEMENT_INTERFACE (VALENT_TYPE_DEVICE_PLUGIN, valent_device_plugin_iface_init))

enum {
  PROP_0,
  PROP_DEVICE,
  N_PROPERTIES
};

/**
 * Packet Handlers
 */
static void
handle_mock_echo (ValentDevicePlugin *plugin,
                  JsonNode           *packet)
{
  ValentMockDevicePlugin *self = VALENT_MOCK_DEVICE_PLUGIN (plugin);
  g_autoptr (JsonNode) response = NULL;
  g_autofree char *packet_json = NULL;

  g_assert (VALENT_IS_DEVICE_PLUGIN (plugin));

  packet_json = json_to_string (packet, TRUE);
  g_message ("%s", packet_json);

  response = json_from_string (packet_json, NULL);
  valent_device_queue_packet (self->device, response);
}

static void
packet_action (GSimpleAction *action,
               GVariant      *parameter,
               gpointer       user_data)
{
  ValentMockDevicePlugin *self = VALENT_MOCK_DEVICE_PLUGIN (user_data);
  JsonBuilder *builder;
  g_autoptr (JsonNode) packet = NULL;

  g_assert (VALENT_IS_MOCK_DEVICE_PLUGIN (self));

  builder = valent_packet_start ("kdeconnect.mock.echo");
  packet = valent_packet_finish (builder);

  valent_device_queue_packet (self->device, packet);
}

static const GActionEntry actions[] = {
    {"test-transfer", packet_action, NULL, NULL, NULL}
};

static const ValentMenuEntry items[] = {
    {"Packet Action", "device.test-transfer", "dialog-information-symbolic"}
};

/**
 * ValentDevicePlugin Implementation
 */
static void
valent_mock_device_plugin_enable (ValentDevicePlugin *plugin)
{
  valent_device_plugin_register_actions (plugin,
                                         actions,
                                         G_N_ELEMENTS (actions));

  valent_device_plugin_add_menu_entries (plugin,
                                         items,
                                         G_N_ELEMENTS (items));
}

static void
valent_mock_device_plugin_disable (ValentDevicePlugin *plugin)
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
valent_mock_device_plugin_update_state (ValentDevicePlugin *plugin)
{
  ValentMockDevicePlugin *self = VALENT_MOCK_DEVICE_PLUGIN (plugin);
  gboolean connected;
  gboolean paired;
  gboolean available;

  connected = valent_device_get_connected (self->device);
  paired = valent_device_get_paired (self->device);
  available = (connected && paired);

  valent_device_plugin_toggle_actions (plugin,
                                       actions,
                                       G_N_ELEMENTS (actions),
                                       available);
}

static void
valent_mock_device_plugin_handle_packet (ValentDevicePlugin *plugin,
                                  const char         *type,
                                  JsonNode           *packet)
{
  g_assert (VALENT_IS_MOCK_DEVICE_PLUGIN (plugin));
  g_assert (type != NULL);
  g_assert (VALENT_IS_PACKET (packet));

  if (g_strcmp0 (type, "kdeconnect.mock.echo") == 0)
    handle_mock_echo (plugin, packet);
  else
    g_assert_not_reached ();
}

static void
valent_device_plugin_iface_init (ValentDevicePluginInterface *iface)
{
  iface->enable = valent_mock_device_plugin_enable;
  iface->disable = valent_mock_device_plugin_disable;
  iface->handle_packet = valent_mock_device_plugin_handle_packet;
  iface->update_state = valent_mock_device_plugin_update_state;
}

/**
 * GObject Implementation
 */
static void
valent_mock_device_plugin_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  ValentMockDevicePlugin *self = VALENT_MOCK_DEVICE_PLUGIN (object);

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
valent_mock_device_plugin_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  ValentMockDevicePlugin *self = VALENT_MOCK_DEVICE_PLUGIN (object);

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
valent_mock_device_plugin_class_init (ValentMockDevicePluginClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = valent_mock_device_plugin_get_property;
  object_class->set_property = valent_mock_device_plugin_set_property;

  g_object_class_override_property (object_class, PROP_DEVICE, "device");
}

static void
valent_mock_device_plugin_init (ValentMockDevicePlugin *self)
{
}

