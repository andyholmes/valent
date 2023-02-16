// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <libvalent-core.h>
#include <libvalent-device.h>

#include "valent-mock-device-plugin.h"


struct _ValentMockDevicePlugin
{
  ValentDevicePlugin  parent_instance;
};

G_DEFINE_FINAL_TYPE (ValentMockDevicePlugin, valent_mock_device_plugin, VALENT_TYPE_DEVICE_PLUGIN)


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

static void
valent_transfer_execute_cb (ValentTransfer *transfer,
                            GAsyncResult   *result,
                            gpointer        user_data)
{
  g_autoptr (ValentDevice) device = NULL;
  g_autoptr (JsonNode) packet = NULL;
  g_autoptr (GFile) file = NULL;
  g_autoptr (GError) error = NULL;

  if (!valent_transfer_execute_finish (transfer, result, &error))
    g_assert_no_error (error);

  g_object_get (transfer,
                "device", &device,
                "file",   &file,
                "packet", &packet,
                NULL);

  g_assert_true (VALENT_IS_DEVICE (device));
  g_assert_true (VALENT_IS_PACKET (packet));
  g_assert_true (G_IS_FILE (file));
}

static void
valent_mock_device_plugin_handle_transfer (ValentMockDevicePlugin *self,
                                           JsonNode               *packet)
{
  g_autoptr (ValentTransfer) transfer = NULL;
  g_autoptr (GCancellable) cancellable = NULL;
  g_autoptr (GFile) file = NULL;
  g_autofree char *directory = NULL;
  ValentDevice *device;
  const char *filename;

  g_assert (VALENT_IS_MOCK_DEVICE_PLUGIN (self));
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
  directory = valent_data_get_directory (G_USER_DIRECTORY_DOWNLOAD);
  file = valent_data_get_file (directory, filename, TRUE);

  /* Create a new transfer */
  transfer = valent_device_transfer_new_for_file (device, packet, file);
  valent_transfer_execute (transfer,
                           cancellable,
                           (GAsyncReadyCallback)valent_transfer_execute_cb,
                           self);
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

/*
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

  if (strcmp (type, "kdeconnect.mock.echo") == 0)
    valent_mock_device_plugin_handle_echo (self, packet);
  else if (strcmp (type, "kdeconnect.mock.transfer") == 0)
    valent_mock_device_plugin_handle_transfer (self, packet);
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

