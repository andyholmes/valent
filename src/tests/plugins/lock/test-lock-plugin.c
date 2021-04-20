#include <gio/gio.h>
#include <libvalent-core.h>
#include <libvalent-test.h>

#include "kdeconnect.lock-fuzz.h"
#include "kdeconnect.lock.request-fuzz.h"


static void
test_lock_plugin_basic (ValentTestPluginFixture *fixture,
                        gconstpointer            user_data)
{
  ValentDevice *device;
  GActionGroup *actions;

  device = valent_test_plugin_fixture_get_device (fixture);
  actions = valent_device_get_actions (device);
  g_assert_true (g_action_group_has_action (actions, "lock"));
  g_assert_true (g_action_group_has_action (actions, "unlock"));
}

static void
test_lock_plugin_handle_request (ValentTestPluginFixture *fixture,
                                 gconstpointer            user_data)
{
  JsonNode *packet;

  valent_test_plugin_fixture_connect (fixture, TRUE);

  /* Receive the remote locked state (requested on connect) */
  packet = valent_test_plugin_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.lock.request");
  v_assert_packet_true (packet, "requestLocked");
  json_node_unref (packet);

  packet = valent_test_plugin_fixture_lookup_packet (fixture, "is-locked");
  valent_test_plugin_fixture_handle_packet (fixture, packet);

  /* Receive a request for the local locked state */
  packet = valent_test_plugin_fixture_lookup_packet (fixture, "request-locked");
  valent_test_plugin_fixture_handle_packet (fixture, packet);

  packet = valent_test_plugin_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.lock");
  v_assert_packet_false (packet, "isLocked");
  json_node_unref (packet);

  /* Receive a lock (message) */
  packet = valent_test_plugin_fixture_lookup_packet (fixture, "set-locked");
  valent_test_plugin_fixture_handle_packet (fixture, packet);

  packet = valent_test_plugin_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.lock");
  v_assert_packet_true (packet, "isLocked");
  json_node_unref (packet);
}

static void
test_lock_plugin_send_request (ValentTestPluginFixture *fixture,
                               gconstpointer            user_data)
{
  ValentDevice *device;
  GActionGroup *actions;
  JsonNode *packet;

  valent_test_plugin_fixture_connect (fixture, TRUE);

  device = valent_test_plugin_fixture_get_device (fixture);
  actions = valent_device_get_actions (device);

  /* expect connect packet */
  packet = valent_test_plugin_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.lock.request");
  v_assert_packet_true (packet, "requestLocked");
  json_node_unref (packet);

  packet = valent_test_plugin_fixture_lookup_packet (fixture, "is-unlocked");
  valent_test_plugin_fixture_handle_packet (fixture, packet);

  /* lock the endpoint */
  g_assert_true (g_action_group_get_action_enabled (actions, "lock"));
  g_action_group_activate_action (actions, "lock", NULL);

  packet = valent_test_plugin_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.lock.request");
  v_assert_packet_true (packet, "setLocked");
  json_node_unref (packet);

  packet = valent_test_plugin_fixture_lookup_packet (fixture, "is-locked");
  valent_test_plugin_fixture_handle_packet (fixture, packet);

  /* lock the endpoint (message) */
  g_assert_true (g_action_group_get_action_enabled (actions, "unlock"));
  g_action_group_activate_action (actions, "unlock", NULL);

  packet = valent_test_plugin_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.lock.request");
  v_assert_packet_false (packet, "setLocked");
  json_node_unref (packet);
}

static void
test_lock_plugin_fuzz (ValentTestPluginFixture *fixture,
                       gconstpointer            user_data)

{
  g_autoptr (JsonParser) parser = NULL;
  JsonNode *packet = NULL;

  valent_test_plugin_fixture_connect (fixture, TRUE);

  parser = json_parser_new ();

  for (unsigned int i = 0; i < G_N_ELEMENTS (lock_fuzz); i++)
    {
      json_parser_load_from_data (parser,
                                  lock_fuzz[i].json,
                                  lock_fuzz[i].size,
                                  NULL);
      packet = json_parser_get_root (parser);

      if (VALENT_IS_PACKET (packet))
        valent_test_plugin_fixture_handle_packet (fixture, packet);
    }

  for (unsigned int i = 0; i < G_N_ELEMENTS (lock_request_fuzz); i++)
    {
      json_parser_load_from_data (parser,
                                  lock_request_fuzz[i].json,
                                  lock_request_fuzz[i].size,
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
  const char *path = TEST_DATA_DIR"/plugin-lock.json";

  g_test_init (&argc, &argv, G_TEST_OPTION_ISOLATE_DIRS, NULL);

  g_test_add ("/plugins/lock/basic",
              ValentTestPluginFixture, path,
              valent_test_plugin_fixture_init,
              test_lock_plugin_basic,
              valent_test_plugin_fixture_clear);

  g_test_add ("/plugins/lock/handle-request",
              ValentTestPluginFixture, path,
              valent_test_plugin_fixture_init,
              test_lock_plugin_handle_request,
              valent_test_plugin_fixture_clear);

  g_test_add ("/plugins/lock/send-request",
              ValentTestPluginFixture, path,
              valent_test_plugin_fixture_init,
              test_lock_plugin_send_request,
              valent_test_plugin_fixture_clear);

#ifdef VALENT_TEST_FUZZ
  g_test_add ("/plugins/lock/fuzz",
              ValentTestPluginFixture, path,
              valent_test_plugin_fixture_init,
              test_lock_plugin_fuzz,
              valent_test_plugin_fixture_clear);
#endif

  return g_test_run ();
}
