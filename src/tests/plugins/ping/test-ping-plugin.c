#include <gio/gio.h>
#include <libvalent-core.h>
#include <libvalent-test.h>

#include "kdeconnect.ping-fuzz.h"


static void
test_ping_plugin_basic (ValentTestPluginFixture *fixture,
                        gconstpointer            user_data)
{
  ValentDevice *device;
  GActionGroup *actions;

  device = valent_test_plugin_fixture_get_device (fixture);
  actions = valent_device_get_actions (device);
  g_assert_true (g_action_group_has_action (actions, "ping"));
}

static void
test_ping_plugin_handle_request (ValentTestPluginFixture *fixture,
                                 gconstpointer            user_data)
{
  JsonNode *packet;

  valent_test_plugin_fixture_connect (fixture, TRUE);

  /* Receive a ping */
  packet = valent_test_plugin_fixture_lookup_packet (fixture, "ping");
  valent_test_plugin_fixture_handle_packet (fixture, packet);

  /* Receive a ping (message) */
  packet = valent_test_plugin_fixture_lookup_packet (fixture, "ping-message");
  valent_test_plugin_fixture_handle_packet (fixture, packet);
}

static void
test_ping_plugin_send_request (ValentTestPluginFixture *fixture,
                               gconstpointer            user_data)
{
  ValentDevice *device;
  GActionGroup *actions;
  JsonNode *packet;

  valent_test_plugin_fixture_connect (fixture, TRUE);

  device = valent_test_plugin_fixture_get_device (fixture);
  actions = valent_device_get_actions (device);
  g_assert_true (g_action_group_get_action_enabled (actions, "ping"));
  g_assert_true (g_action_group_get_action_enabled (actions, "ping-message"));

  /* Ping the endpoint */
  g_action_group_activate_action (actions, "ping", NULL);

  packet = valent_test_plugin_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.ping");
  json_node_unref (packet);

  /* Ping the endpoint (message) */
  g_action_group_activate_action (actions, "ping-message",
                                  g_variant_new_string ("Test"));

  packet = valent_test_plugin_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.ping");
  v_assert_packet_field (packet, "message");
  json_node_unref (packet);
}

static void
test_ping_plugin_fuzz (ValentTestPluginFixture *fixture,
                       gconstpointer            user_data)

{
  g_autoptr (JsonParser) parser = NULL;
  JsonNode *packet = NULL;

  valent_test_plugin_fixture_connect (fixture, TRUE);

  parser = json_parser_new ();

  for (unsigned int i = 0; i < G_N_ELEMENTS (ping_fuzz); i++)
    {
      json_parser_load_from_data (parser,
                                  ping_fuzz[i].json,
                                  ping_fuzz[i].size,
                                  NULL);
      packet = json_parser_get_root (parser);

      if (VALENT_IS_PACKET (packet))
        valent_test_plugin_fixture_handle_packet (fixture, packet);
    }
}

int
main (int   argc,
      char *argv[])
{
  const char *path = TEST_DATA_DIR"/plugin-ping.json";

  g_test_init (&argc, &argv, G_TEST_OPTION_ISOLATE_DIRS, NULL);

  g_test_add ("/plugins/ping/basic",
              ValentTestPluginFixture, path,
              valent_test_plugin_fixture_init,
              test_ping_plugin_basic,
              valent_test_plugin_fixture_clear);

  g_test_add ("/plugins/ping/handle-request",
              ValentTestPluginFixture, path,
              valent_test_plugin_fixture_init,
              test_ping_plugin_handle_request,
              valent_test_plugin_fixture_clear);

  g_test_add ("/plugins/ping/send-request",
              ValentTestPluginFixture, path,
              valent_test_plugin_fixture_init,
              test_ping_plugin_send_request,
              valent_test_plugin_fixture_clear);

#ifdef VALENT_TEST_FUZZ
  g_test_add ("/plugins/ping/fuzz",
              ValentTestPluginFixture, path,
              valent_test_plugin_fixture_init,
              test_ping_plugin_fuzz,
              valent_test_plugin_fixture_clear);
#endif

  return g_test_run ();
}
