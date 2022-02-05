// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>
#include <libvalent-core.h>
#include <libvalent-test.h>

#include "valent-device-private.h"


typedef struct
{
  GMainLoop     *loop;
  ValentDevice  *device;
  ValentChannel *channel;
  ValentChannel *endpoint;
  JsonNode      *packets;
} DeviceFixture;


static inline JsonNode *
get_packet (DeviceFixture *fixture,
            const char    *name)
{
  return json_object_get_member (json_node_get_object (fixture->packets), name);
}

static void
device_fixture_set_up (DeviceFixture *fixture,
                       gconstpointer  user_data)
{
  g_autofree ValentChannel **channels = NULL;
  JsonNode *identity;

  fixture->loop = g_main_loop_new (NULL, FALSE);
  fixture->packets = valent_test_load_json (TEST_DATA_DIR"/core.json");

  /* Init device */
  identity = get_packet (fixture, "identity");
  fixture->device = valent_device_new (identity);

  /* Init Channels */
  channels = valent_test_channels (identity, identity);
  fixture->channel = g_steal_pointer (&channels[0]);
  fixture->endpoint = g_steal_pointer (&channels[1]);
}

static void
device_fixture_tear_down (DeviceFixture *fixture,
                          gconstpointer  user_data)
{
  valent_channel_close (fixture->endpoint, NULL, NULL);
  v_await_finalize_object (fixture->endpoint);
  v_await_finalize_object (fixture->device);
  v_await_finalize_object (fixture->channel);

  g_clear_pointer (&fixture->packets, json_node_unref);
  g_clear_pointer (&fixture->loop, g_main_loop_unref);
}

/*
 * Packet Helpers
 */
static void
endpoint_expect_packet_cb (ValentChannel  *channel,
                           GAsyncResult   *result,
                           JsonNode      **packet)
{
  g_autoptr (GError) error = NULL;

  *packet = valent_channel_read_packet_finish (channel, result, &error);
  g_assert_no_error (error);
}

static void
endpoint_expect_packet_pair (DeviceFixture *fixture,
                             gboolean       pair)
{
  g_autoptr (JsonNode) packet = NULL;

  valent_channel_read_packet (fixture->endpoint,
                              NULL,
                              (GAsyncReadyCallback)endpoint_expect_packet_cb,
                              &packet);

  while (packet == NULL)
    g_main_context_iteration (NULL, FALSE);

  v_assert_packet_type (packet, "kdeconnect.pair");
  v_assert_packet_field (packet, "pair");

  if (pair)
    v_assert_packet_true (packet, "pair");
  else
    v_assert_packet_false (packet, "pair");
}

static void
endpoint_expect_packet_echo (DeviceFixture *fixture,
                             JsonNode      *packet)
{
  g_autoptr (JsonNode) echo = NULL;

  valent_channel_read_packet (fixture->endpoint,
                              NULL,
                              (GAsyncReadyCallback)endpoint_expect_packet_cb,
                              &echo);

  while (echo == NULL)
    g_main_context_iteration (NULL, FALSE);

  v_assert_packet_type (echo, "kdeconnect.mock.echo");
  v_assert_packet_field (echo, "foo");
  v_assert_packet_cmpstr (echo, "foo", ==, "bar");
}


/*
 * First test constructing a device before using the fixture
 */
static void
test_device_new (void)
{
  ValentDevice *device = NULL;

  gboolean connected;
  g_autofree char *icon_name = NULL;
  g_autofree char *id = NULL;
  g_autofree char *name = NULL;
  g_autofree char *type = NULL;
  gboolean paired;

  GMenuModel *menu;
  GPtrArray *plugins;

  device = g_object_new (VALENT_TYPE_DEVICE,
                         "id", "test-device",
                         NULL);
  g_assert_true (VALENT_IS_DEVICE (device));

  g_object_get (device,
                "id",        &id,
                "icon-name", &icon_name,
                "name",      &name,
                "type",      &type,
                "connected", &connected,
                "paired",    &paired,
                NULL);

  /* id should be set, but everything else should be %FALSE or %NULL */
  g_assert_cmpstr (id, ==, "test-device");
  g_assert_null (icon_name);
  g_assert_null (type);
  g_assert_null (name);
  g_assert_false (connected);
  g_assert_false (paired);

  menu = valent_device_get_menu (device);
  g_assert_true (G_IS_MENU (menu));

  /* Only "Packetless" plugin should be loaded */
  plugins = valent_device_get_plugins (device);
  g_assert_cmpuint (plugins->len, ==, 1);
  g_ptr_array_unref (plugins);

  v_assert_finalize_object (device);
}

/*
 * Now test handling an identity packet with the fixture
 */
static void
test_device_basic (DeviceFixture *fixture,
                   gconstpointer  user_data)
{
  g_autoptr (ValentData) data = NULL;
  g_autofree char *id = NULL;
  g_autofree char *name = NULL;
  g_autofree char *icon_name = NULL;
  g_autofree char *type = NULL;
  gboolean connected;
  gboolean paired;
  GPtrArray *plugins;

  /* Test properties */
  g_object_get (fixture->device,
                "data",             &data,
                "id",               &id,
                "name",             &name,
                "icon-name",        &icon_name,
                "type",             &type,
                "connected",        &connected,
                "paired",           &paired,
                NULL);

  g_assert_true (VALENT_IS_DATA (data));
  g_assert_cmpstr (id, ==, "test-device");
  g_assert_cmpstr (name, ==, "Test Device");
  g_assert_cmpstr (icon_name, ==, "smartphone-symbolic");
  g_assert_cmpstr (type, ==, "phone");
  g_assert_false (connected);
  g_assert_false (paired);

  /* "Packetless" and "Test" plugins should be loaded */
  plugins = valent_device_get_plugins (fixture->device);
  g_assert_cmpuint (plugins->len, ==, 2);
  g_ptr_array_unref (plugins);
}

static void
test_device_connecting (DeviceFixture *fixture,
                        gconstpointer  user_data)
{
  /* Connect */
  valent_device_set_channel (fixture->device, fixture->channel);
  g_assert_true (valent_device_get_connected (fixture->device));

  /* Disconnect */
  valent_device_set_channel (fixture->device, NULL);
  g_assert_false (valent_device_get_connected (fixture->device));
}

/*
 * Test pairing
 */
static void
test_device_pairing (DeviceFixture *fixture,
                     gconstpointer  user_data)
{
  JsonNode *pair, *unpair;
  GActionGroup *actions;

  pair = get_packet (fixture, "pair");
  unpair = get_packet (fixture, "unpair");
  actions = valent_device_get_actions (fixture->device);

  /* Attach channel */
  valent_device_set_channel (fixture->device, fixture->channel);
  g_assert_true (valent_device_get_connected (fixture->device));
  g_assert_false (valent_device_get_paired (fixture->device));


  /* Send Pair (Request), Receive Unpair (Reject) */
  g_action_group_activate_action (actions, "pair", NULL);
  endpoint_expect_packet_pair (fixture, TRUE);
  valent_device_handle_packet (fixture->device, unpair);
  g_assert_false (valent_device_get_paired (fixture->device));


  /* Send Pair (Request), Receive Pair (Accept) */
  g_action_group_activate_action (actions, "pair", NULL);
  endpoint_expect_packet_pair (fixture, TRUE);
  valent_device_handle_packet (fixture->device, pair);
  g_assert_true (valent_device_get_paired (fixture->device));


  /* Receive Pair (Request), Auto-confirm Pair */
  valent_device_handle_packet (fixture->device, pair);
  endpoint_expect_packet_pair (fixture, TRUE);
  g_assert_true (valent_device_get_paired (fixture->device));

  valent_device_set_paired (fixture->device, FALSE);
  g_assert_false (valent_device_get_paired (fixture->device));


  /* Receive Pair (Request), Send Unpair (Reject) */
  valent_device_handle_packet (fixture->device, pair);
  g_assert_false (valent_device_get_paired (fixture->device));

  g_action_group_activate_action (actions, "unpair", NULL);
  endpoint_expect_packet_pair (fixture, FALSE);
  g_assert_false (valent_device_get_paired (fixture->device));


  /* Receive Pair (Request), Send Pair (Accept), Send Unpair */
  valent_device_handle_packet (fixture->device, pair);
  g_assert_false (valent_device_get_paired (fixture->device));

  g_action_group_activate_action (actions, "pair", NULL);
  endpoint_expect_packet_pair (fixture, TRUE);
  g_assert_true (valent_device_get_paired (fixture->device));

  g_action_group_activate_action (actions, "unpair", NULL);
  endpoint_expect_packet_pair (fixture, FALSE);
  g_assert_false (valent_device_get_paired (fixture->device));


  /* Detach channel */
  valent_device_set_channel (fixture->device, NULL);
  g_assert_false (valent_device_get_connected (fixture->device));
}

/*
 * Device Plugins
 */
static void
toggle_plugin (PeasPluginInfo *info,
               ValentDevice   *device)
{
  g_autofree char *path = NULL;
  g_autoptr (GSettings) settings = NULL;
  gboolean enabled;

  g_assert_true (VALENT_IS_DEVICE (device));
  g_assert_nonnull (info);

  path = g_strdup_printf ("/ca/andyholmes/valent/device/%s/plugin/%s/",
                          valent_device_get_id (device),
                          peas_plugin_info_get_module_name (info));
  settings = g_settings_new_with_path ("ca.andyholmes.Valent.Plugin", path);

  enabled = g_settings_get_boolean (settings, "enabled");
  g_settings_set_boolean (settings, "enabled", !enabled);
}

static void
test_device_plugins (DeviceFixture *fixture,
                     gconstpointer  user_data)
{
  PeasEngine *engine;
  GPtrArray *device_plugins;
  const GList *engine_plugins;

  /* Plugins should be loaded */
  device_plugins = valent_device_get_plugins (fixture->device);
  g_assert_cmpuint (device_plugins->len, >, 0);
  g_ptr_array_unref (device_plugins);

  /* Unload & Load Plugins (Engine) */
  engine = valent_get_engine ();
  engine_plugins = peas_engine_get_plugin_list (engine);

  /* Unload Plugins */
  for (const GList *iter = engine_plugins; iter; iter = iter->next)
    peas_engine_unload_plugin (engine, iter->data);

  device_plugins = valent_device_get_plugins (fixture->device);
  g_assert_cmpuint (device_plugins->len, ==, 0);
  g_ptr_array_unref (device_plugins);

  /* Load Plugins */
  for (const GList *iter = engine_plugins; iter; iter = iter->next)
    peas_engine_load_plugin (engine, iter->data);

  device_plugins = valent_device_get_plugins (fixture->device);
  g_assert_cmpuint (device_plugins->len, >, 0);

  /* Enable/Disable Plugins */
  g_ptr_array_foreach (device_plugins, (GFunc)toggle_plugin, fixture->device);
  g_ptr_array_foreach (device_plugins, (GFunc)toggle_plugin, fixture->device);
  g_ptr_array_unref (device_plugins);
}

/*
 * Packet Handling
 */
static void
test_handle_packet (DeviceFixture *fixture,
                    gconstpointer  user_data)
{
  JsonNode *packet = get_packet (fixture, "test-echo");

  valent_device_set_channel (fixture->device, fixture->channel);
  g_assert_true (valent_device_get_connected (fixture->device));

  /* Local device is paired, we expect to receive the echo */
  valent_device_set_paired (fixture->device, TRUE);
  g_assert_true (valent_device_get_paired (fixture->device));

  valent_channel_write_packet (fixture->endpoint, packet, NULL, NULL, NULL);
  endpoint_expect_packet_echo (fixture, packet);

  /* Local device is unpaired, we expect to receive a pair packet informing us
   * that the device is unpaired. */
  valent_device_set_paired (fixture->device, FALSE);
  g_assert_false (valent_device_get_paired (fixture->device));

  valent_channel_write_packet (fixture->endpoint, packet, NULL, NULL, NULL);
  endpoint_expect_packet_pair (fixture, FALSE);
}

static void
test_queue_packet_available (DeviceFixture *fixture,
                             gconstpointer  user_data)
{
  if (g_test_subprocess ())
    {
      JsonNode *pair = get_packet (fixture, "pair");

      valent_device_set_channel (fixture->device, fixture->channel);
      g_assert_true (valent_device_get_connected (fixture->device));
      valent_device_set_paired (fixture->device, TRUE);
      g_assert_true (valent_device_get_paired (fixture->device));

      valent_device_queue_packet (fixture->device, pair);
      endpoint_expect_packet_pair (fixture, TRUE);

      valent_device_set_channel (fixture->device, NULL);
      g_assert_false (valent_device_get_connected (fixture->device));

      return;
    }
  g_test_trap_subprocess (NULL, 0, 0);
  g_test_trap_assert_passed ();
}

static void
test_queue_packet_disconnected (DeviceFixture *fixture,
                                gconstpointer  user_data)
{
  if (g_test_subprocess ())
    {
      JsonNode *pair = get_packet (fixture, "pair");

      valent_device_set_channel (fixture->device, NULL);
      g_assert_false (valent_device_get_connected (fixture->device));
      valent_device_set_paired (fixture->device, TRUE);
      g_assert_true (valent_device_get_paired (fixture->device));

      valent_device_queue_packet (fixture->device, pair);

      valent_device_set_channel (fixture->device, NULL);
      g_assert_false (valent_device_get_connected (fixture->device));

      return;
    }
  g_test_trap_subprocess (NULL, 0, 0);
  g_test_trap_assert_failed ();
}

static void
test_queue_packet_unpaired (DeviceFixture *fixture,
                            gconstpointer  user_data)
{
  if (g_test_subprocess ())
    {
      JsonNode *pair = get_packet (fixture, "pair");

      valent_device_set_channel (fixture->device, fixture->channel);
      g_assert_true (valent_device_get_connected (fixture->device));
      valent_device_set_paired (fixture->device, FALSE);
      g_assert_false (valent_device_get_paired (fixture->device));

      valent_device_queue_packet (fixture->device, pair);

      valent_device_set_channel (fixture->device, NULL);
      g_assert_false (valent_device_get_connected (fixture->device));

      return;
    }
  g_test_trap_subprocess (NULL, 0, 0);
  g_test_trap_assert_failed ();
}

static void
send_available_cb (ValentDevice  *device,
                   GAsyncResult  *result,
                   DeviceFixture *fixture)
{
  g_autoptr (GError) error = NULL;

  valent_device_send_packet_finish (device, result, &error);
  g_assert_no_error (error);

  g_main_loop_quit (fixture->loop);
}

static void
send_disconnected_cb (ValentDevice  *device,
                      GAsyncResult  *result,
                      DeviceFixture *fixture)
{
  g_autoptr (GError) error = NULL;

  valent_device_send_packet_finish (device, result, &error);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_NOT_CONNECTED);

  g_main_loop_quit (fixture->loop);
}

static void
send_unpaired_cb (ValentDevice  *device,
                  GAsyncResult  *result,
                  DeviceFixture *fixture)
{
  g_autoptr (GError) error = NULL;

  valent_device_send_packet_finish (device, result, &error);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED);

  g_main_loop_quit (fixture->loop);
}

static void
test_send_packet (DeviceFixture *fixture,
                  gconstpointer  user_data)
{
  JsonNode *pair = get_packet (fixture, "pair");

  /* Disconnected & Paired */
  g_assert_false (valent_device_get_connected (fixture->device));

  valent_device_set_paired (fixture->device, TRUE);
  g_assert_true (valent_device_get_paired (fixture->device));

  valent_device_send_packet (fixture->device,
                             pair,
                             NULL,
                             (GAsyncReadyCallback)send_disconnected_cb,
                             fixture);
  g_main_loop_run (fixture->loop);

  /* Connected & Paired */
  valent_device_set_channel (fixture->device, fixture->channel);
  g_assert_true (valent_device_get_connected (fixture->device));

  valent_device_set_paired (fixture->device, TRUE);
  g_assert_true (valent_device_get_paired (fixture->device));

  valent_device_send_packet (fixture->device,
                             pair,
                             NULL,
                             (GAsyncReadyCallback)send_available_cb,
                             fixture);
  g_main_loop_run (fixture->loop);
  endpoint_expect_packet_pair (fixture, TRUE);

  /* Connected & Unpaired */
  valent_device_set_channel (fixture->device, fixture->channel);
  g_assert_true (valent_device_get_connected (fixture->device));

  valent_device_set_paired (fixture->device, FALSE);
  g_assert_false (valent_device_get_paired (fixture->device));

  valent_device_send_packet (fixture->device,
                             pair,
                             NULL,
                             (GAsyncReadyCallback)send_unpaired_cb,
                             fixture);
  g_main_loop_run (fixture->loop);

  /* Cleanup */
  valent_device_set_channel (fixture->device, NULL);
  g_assert_false (valent_device_get_connected (fixture->device));
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, G_TEST_OPTION_ISOLATE_DIRS, NULL);

  g_test_add_func ("/core/device/new",
                   test_device_new);

  g_test_add ("/core/device/basic",
              DeviceFixture, NULL,
              device_fixture_set_up,
              test_device_basic,
              device_fixture_tear_down);

  g_test_add ("/core/device/connecting",
              DeviceFixture, NULL,
              device_fixture_set_up,
              test_device_connecting,
              device_fixture_tear_down);

  g_test_add ("/core/device/pairing",
              DeviceFixture, NULL,
              device_fixture_set_up,
              test_device_pairing,
              device_fixture_tear_down);

  g_test_add ("/core/device/plugins",
              DeviceFixture, NULL,
              device_fixture_set_up,
              test_device_plugins,
              device_fixture_tear_down);

  g_test_add ("/core/device/handle-packet",
              DeviceFixture, NULL,
              device_fixture_set_up,
              test_handle_packet,
              device_fixture_tear_down);

  g_test_add ("/core/device/queue-packet-available",
              DeviceFixture, NULL,
              device_fixture_set_up,
              test_queue_packet_available,
              device_fixture_tear_down);

  g_test_add ("/core/device/queue-packet-disconnected",
              DeviceFixture, NULL,
              device_fixture_set_up,
              test_queue_packet_disconnected,
              device_fixture_tear_down);

  g_test_add ("/core/device/queue-packet-unpaired",
              DeviceFixture, NULL,
              device_fixture_set_up,
              test_queue_packet_unpaired,
              device_fixture_tear_down);

  g_test_add ("/core/device/send-packet",
              DeviceFixture, NULL,
              device_fixture_set_up,
              test_send_packet,
              device_fixture_tear_down);

  return g_test_run ();
}

