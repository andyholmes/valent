// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>
#include <gtk/gtk.h>
#include <valent.h>
#include <libvalent-test.h>


static void
test_sms_plugin_basic (ValentTestFixture *fixture,
                       gconstpointer      user_data)
{
  GActionGroup *actions = G_ACTION_GROUP (fixture->device);
  JsonNode *packet;

  VALENT_TEST_CHECK ("Plugin has expected actions");
  g_assert_true (g_action_group_has_action (actions, "sms.fetch"));
  g_assert_true (g_action_group_has_action (actions, "sms.messaging"));

  valent_test_fixture_connect (fixture, TRUE);

  VALENT_TEST_CHECK ("Plugin actions are enabled when connected");
  g_assert_true (g_action_group_get_action_enabled (actions, "sms.fetch"));
  g_assert_true (g_action_group_get_action_enabled (actions, "sms.messaging"));

  VALENT_TEST_CHECK ("Plugin requests the threads on connect");
  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.sms.request_conversations");
  json_node_unref (packet);

  VALENT_TEST_CHECK ("Plugin action `sms.fetch` sends a request for the thread list");
  g_action_group_activate_action (actions, "sms.fetch", NULL);
  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.sms.request_conversations");
  json_node_unref (packet);

  VALENT_TEST_CHECK ("Plugin action `sms.messaging` opens the messaging window");
  g_action_group_activate_action (actions, "sms.messaging", NULL);
}

static void
test_sms_plugin_handle_request (ValentTestFixture *fixture,
                                gconstpointer      user_data)
{
  JsonNode *packet;

  valent_test_fixture_connect (fixture, TRUE);

  VALENT_TEST_CHECK ("Plugin requests the threads on connect");
  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.sms.request_conversations");
  json_node_unref (packet);

  VALENT_TEST_CHECK ("Plugin handles the latest thread message (1)");
  packet = valent_test_fixture_lookup_packet (fixture, "connect-time-1");
  valent_test_fixture_handle_packet (fixture, packet);

  VALENT_TEST_CHECK ("Plugin requests the thread (1)");
  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.sms.request_conversation");
  v_assert_packet_cmpint (packet, "threadID", ==, 1);
  json_node_unref (packet);

  VALENT_TEST_CHECK ("Plugin handles the requested thread (1)");
  packet = valent_test_fixture_lookup_packet (fixture, "thread-1");
  valent_test_fixture_handle_packet (fixture, packet);

  VALENT_TEST_CHECK ("Plugin handles the latest thread message (2)");
  packet = valent_test_fixture_lookup_packet (fixture, "connect-time-2");
  valent_test_fixture_handle_packet (fixture, packet);

  VALENT_TEST_CHECK ("Plugin requests the thread (2)");
  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.sms.request_conversation");
  v_assert_packet_cmpint (packet, "threadID", ==, 2);
  json_node_unref (packet);

  VALENT_TEST_CHECK ("Plugin handles the requested thread (2)");
  packet = valent_test_fixture_lookup_packet (fixture, "thread-2");
  valent_test_fixture_handle_packet (fixture, packet);
}

static const char *schemas[] = {
  "/tests/kdeconnect.sms.attachment_file.json",
  /* "/tests/kdeconnect.sms.messages.json", */
  /* "/tests/kdeconnect.sms.request.json", */
  "/tests/kdeconnect.sms.request_attachment.json",
  "/tests/kdeconnect.sms.request_conversation.json",
  "/tests/kdeconnect.sms.request_conversations.json",
};

static void
test_sms_plugin_fuzz (ValentTestFixture *fixture,
                      gconstpointer      user_data)

{
  valent_test_fixture_connect (fixture, TRUE);
  g_test_log_set_fatal_handler (valent_test_mute_fuzzing, NULL);

  for (size_t s = 0; s < G_N_ELEMENTS (schemas); s++)
    valent_test_fixture_schema_fuzz (fixture, schemas[s]);
}

int
main (int   argc,
      char *argv[])
{
  const char *path = "plugin-sms.json";

  valent_test_ui_init (&argc, &argv, NULL);

  g_test_add ("/plugins/sms/basic",
              ValentTestFixture, path,
              valent_test_fixture_init,
              test_sms_plugin_basic,
              valent_test_fixture_clear);

  g_test_add ("/plugins/sms/handle-request",
              ValentTestFixture, path,
              valent_test_fixture_init,
              test_sms_plugin_handle_request,
              valent_test_fixture_clear);

  g_test_add ("/plugins/sms/fuzz",
              ValentTestFixture, path,
              valent_test_fixture_init,
              test_sms_plugin_fuzz,
              valent_test_fixture_clear);

  return g_test_run ();
}
