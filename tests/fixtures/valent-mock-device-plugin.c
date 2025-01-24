// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <valent.h>

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
  const char *directory = NULL;
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
      g_debug ("%s(): expected \"filename\" field holding a string",
               G_STRFUNC);
      return;
    }

  device = valent_resource_get_source (VALENT_RESOURCE (self));
  cancellable = valent_object_ref_cancellable (VALENT_OBJECT (self));
  directory = valent_get_user_directory (G_USER_DIRECTORY_DOWNLOAD);
  file = valent_get_user_file (directory, filename, TRUE);

  /* Create a new transfer */
  transfer = valent_device_transfer_new (device, packet, file);
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

/*
 * ValentDevicePlugin
 */
static void
valent_mock_device_plugin_update_state (ValentDevicePlugin *plugin,
                                        ValentDeviceState   state)
{
  ValentMockDevicePlugin *self = VALENT_MOCK_DEVICE_PLUGIN (plugin);
  gboolean available;

  g_assert (VALENT_IS_MOCK_DEVICE_PLUGIN (self));

  available = (state & VALENT_DEVICE_STATE_CONNECTED) != 0 &&
              (state & VALENT_DEVICE_STATE_PAIRED) != 0;

  valent_extension_toggle_actions (VALENT_EXTENSION (plugin), available);
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

  if (g_str_equal (type, "kdeconnect.mock.echo"))
    valent_mock_device_plugin_handle_echo (self, packet);
  else if (g_str_equal (type, "kdeconnect.mock.transfer"))
    valent_mock_device_plugin_handle_transfer (self, packet);
  else
    g_assert_not_reached ();
}

/*
 * ValentObject
 */
static void
valent_mock_device_plugin_destroy (ValentObject *object)
{
  ValentDevicePlugin *plugin = VALENT_DEVICE_PLUGIN (object);

  valent_device_plugin_set_menu_item (plugin, "device.mock.transfer", NULL);

  VALENT_OBJECT_CLASS (valent_mock_device_plugin_parent_class)->destroy (object);
}

/*
 * GObject
 */
static void
valent_mock_device_plugin_constructed (GObject *object)
{
  ValentDevicePlugin *plugin = VALENT_DEVICE_PLUGIN (object);

  G_OBJECT_CLASS (valent_mock_device_plugin_parent_class)->constructed (object);

  g_action_map_add_action_entries (G_ACTION_MAP (plugin),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   plugin);
  valent_device_plugin_set_menu_action (plugin,
                                        "device.mock.transfer",
                                        "Packet Action",
                                        "dialog-information-symbolic");
}

static void
valent_mock_device_plugin_class_init (ValentMockDevicePluginClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentObjectClass *vobject_class = VALENT_OBJECT_CLASS (klass);
  ValentDevicePluginClass *plugin_class = VALENT_DEVICE_PLUGIN_CLASS (klass);

  object_class->constructed = valent_mock_device_plugin_constructed;

  vobject_class->destroy = valent_mock_device_plugin_destroy;

  plugin_class->handle_packet = valent_mock_device_plugin_handle_packet;
  plugin_class->update_state = valent_mock_device_plugin_update_state;
}

static void
valent_mock_device_plugin_init (ValentMockDevicePlugin *self)
{
}

