// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>
#include <valent.h>
#include <libvalent-test.h>


static void
test_lock_plugin_basic (ValentTestFixture *fixture,
                        gconstpointer      user_data)
{
  GActionGroup *actions = G_ACTION_GROUP (fixture->device);

  g_assert_true (g_action_group_has_action (actions, "lock.state"));
}

static void
test_lock_plugin_handle_request (ValentTestFixture *fixture,
                                 gconstpointer      user_data)
{
  JsonNode *packet;

  valent_test_fixture_connect (fixture, TRUE);

  /* Receive the remote locked state (requested on connect) */
  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.lock.request");
  v_assert_packet_true (packet, "requestLocked");
  json_node_unref (packet);

  packet = valent_test_fixture_lookup_packet (fixture, "is-locked");
  valent_test_fixture_handle_packet (fixture, packet);

  /* Receive a request for the local locked state */
  packet = valent_test_fixture_lookup_packet (fixture, "request-locked");
  valent_test_fixture_handle_packet (fixture, packet);

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.lock");
  v_assert_packet_false (packet, "isLocked");
  json_node_unref (packet);

  /* Receive a lock (message) */
  packet = valent_test_fixture_lookup_packet (fixture, "set-locked");
  valent_test_fixture_handle_packet (fixture, packet);

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.lock");
  v_assert_packet_true (packet, "isLocked");
  json_node_unref (packet);
}

static void
test_lock_plugin_send_request (ValentTestFixture *fixture,
                               gconstpointer      user_data)
{
  GActionGroup *actions = G_ACTION_GROUP (fixture->device);
  JsonNode *packet;

  valent_test_fixture_connect (fixture, TRUE);

  /* expect connect packet */
  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.lock.request");
  v_assert_packet_true (packet, "requestLocked");
  json_node_unref (packet);

  packet = valent_test_fixture_lookup_packet (fixture, "is-unlocked");
  valent_test_fixture_handle_packet (fixture, packet);

  /* lock the endpoint */
  g_assert_true (g_action_group_get_action_enabled (actions, "lock.state"));
  g_action_group_change_action_state (actions, "lock.state",
                                      g_variant_new_boolean (TRUE));

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.lock.request");
  v_assert_packet_true (packet, "setLocked");
  json_node_unref (packet);

  packet = valent_test_fixture_lookup_packet (fixture, "is-locked");
  valent_test_fixture_handle_packet (fixture, packet);

  /* lock the endpoint (message) */
  g_assert_true (g_action_group_get_action_enabled (actions, "lock.state"));
  g_action_group_change_action_state (actions, "lock.state",
                                      g_variant_new_boolean (FALSE));

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.lock.request");
  v_assert_packet_false (packet, "setLocked");
  json_node_unref (packet);
}

static const char *schemas[] = {
  JSON_SCHEMA_DIR"/kdeconnect.lock.json",
  JSON_SCHEMA_DIR"/kdeconnect.lock.request.json",
};

static void
test_lock_plugin_fuzz (ValentTestFixture *fixture,
                       gconstpointer      user_data)

{
  valent_test_fixture_connect (fixture, TRUE);
  g_test_log_set_fatal_handler (valent_test_mute_fuzzing, NULL);

  for (unsigned int s = 0; s < G_N_ELEMENTS (schemas); s++)
    valent_test_fixture_schema_fuzz (fixture, schemas[s]);
}

int
main (int   argc,
      char *argv[])
{
  const char *path = "plugin-lock.json";

  valent_test_init (&argc, &argv, NULL);

  g_test_add ("/plugins/lock/basic",
              ValentTestFixture, path,
              valent_test_fixture_init,
              test_lock_plugin_basic,
              valent_test_fixture_clear);

  g_test_add ("/plugins/lock/handle-request",
              ValentTestFixture, path,
              valent_test_fixture_init,
              test_lock_plugin_handle_request,
              valent_test_fixture_clear);

  g_test_add ("/plugins/lock/send-request",
              ValentTestFixture, path,
              valent_test_fixture_init,
              test_lock_plugin_send_request,
              valent_test_fixture_clear);

  g_test_add ("/plugins/lock/fuzz",
              ValentTestFixture, path,
              valent_test_fixture_init,
              test_lock_plugin_fuzz,
              valent_test_fixture_clear);

  return g_test_run ();
}
