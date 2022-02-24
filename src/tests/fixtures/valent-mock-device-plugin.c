// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <libvalent-core.h>

#include "valent-mock-device-plugin.h"


struct _ValentMockDevicePlugin
{
  ValentDevicePlugin  parent_instance;
};

G_DEFINE_TYPE (ValentMockDevicePlugin, valent_mock_device_plugin, VALENT_TYPE_DEVICE_PLUGIN)

/*
 * Packet Handlers
 */
static void
valent_mock_device_plugin_handle_echo (ValentMockDevicePlugin *self,
                                       JsonNode               *packet)
{
  g_autoptr (JsonNode) response = NULL;
  g_autofree char *packet_json = NULL;

  g_assert (VALENT_IS_MOCK_DEVICE_PLUGIN (self));
  g_assert (VALENT_IS_PACKET (packet));

  packet_json = json_to_string (packet, TRUE);
  g_message ("Received echo: %s", packet_json);

  response = json_from_string (packet_json, NULL);
  valent_device_plugin_queue_packet (VALENT_DEVICE_PLUGIN (self), response);
}

/*
 * GActions
 */
static void
echo_action (GSimpleAction *action,
             GVariant      *parameter,
             gpointer       user_data)
{
  ValentMockDevicePlugin *self = VALENT_MOCK_DEVICE_PLUGIN (user_data);
  g_autoptr (JsonNode) packet = NULL;

  g_assert (VALENT_IS_MOCK_DEVICE_PLUGIN (self));

  packet = valent_packet_new ("kdeconnect.mock.echo");
  valent_device_plugin_queue_packet (VALENT_DEVICE_PLUGIN (self), packet);
}

static void
state_action (GSimpleAction *action,
              GVariant      *parameter,
              gpointer       user_data)
{
  ValentMockDevicePlugin *self = VALENT_MOCK_DEVICE_PLUGIN (user_data);

  g_assert (VALENT_IS_MOCK_DEVICE_PLUGIN (self));

  g_simple_action_set_state (action, parameter);
}

static const GActionEntry actions[] = {
    {"echo",  echo_action, NULL, NULL,   NULL},
    {"state", NULL,        NULL, "true", state_action}
};

static const ValentMenuEntry items[] = {
    {"Packet Action", "device.mock.transfer", "dialog-information-symbolic"}
};

/**
 * ValentDevicePlugin
 */
static void
valent_mock_device_plugin_enable (ValentDevicePlugin *plugin)
{
  g_assert (VALENT_IS_MOCK_DEVICE_PLUGIN (plugin));

  g_action_map_add_action_entries (G_ACTION_MAP (plugin),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   plugin);
  valent_device_plugin_add_menu_entries (plugin,
                                         items,
                                         G_N_ELEMENTS (items));
}

static void
valent_mock_device_plugin_disable (ValentDevicePlugin *plugin)
{
  g_assert (VALENT_IS_MOCK_DEVICE_PLUGIN (plugin));

  valent_device_plugin_remove_menu_entries (plugin,
                                            items,
                                            G_N_ELEMENTS (items));
}

static void
valent_mock_device_plugin_update_state (ValentDevicePlugin *plugin,
                                        ValentDeviceState   state)
{
  ValentMockDevicePlugin *self = VALENT_MOCK_DEVICE_PLUGIN (plugin);
  gboolean available;

  g_assert (VALENT_IS_MOCK_DEVICE_PLUGIN (self));

  available = (state & VALENT_DEVICE_STATE_CONNECTED) != 0 &&
              (state & VALENT_DEVICE_STATE_PAIRED) != 0;

  valent_device_plugin_toggle_actions (plugin, available);
}

static void
valent_mock_device_plugin_handle_packet (ValentDevicePlugin *plugin,
                                         const char         *type,
                                         JsonNode           *packet)
{
  ValentMockDevicePlugin *self = VALENT_MOCK_DEVICE_PLUGIN (plugin);

  g_assert (VALENT_IS_MOCK_DEVICE_PLUGIN (plugin));
  g_assert (type != NULL);
  g_assert (VALENT_IS_PACKET (packet));

  if (g_strcmp0 (type, "kdeconnect.mock.echo") == 0)
    valent_mock_device_plugin_handle_echo (self, packet);
  else
    g_assert_not_reached ();
}

/*
 * GObject
 */
static void
valent_mock_device_plugin_class_init (ValentMockDevicePluginClass *klass)
{
  ValentDevicePluginClass *plugin_class = VALENT_DEVICE_PLUGIN_CLASS (klass);

  plugin_class->enable = valent_mock_device_plugin_enable;
  plugin_class->disable = valent_mock_device_plugin_disable;
  plugin_class->handle_packet = valent_mock_device_plugin_handle_packet;
  plugin_class->update_state = valent_mock_device_plugin_update_state;
}

static void
valent_mock_device_plugin_init (ValentMockDevicePlugin *self)
{
}

