// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>
#include <gtk/gtk.h>
#include <libvalent-core.h>
#include <libvalent-input.h>
#include <libvalent-test.h>


static void
test_sms_plugin_basic (ValentTestFixture *fixture,
                       gconstpointer      user_data)
{
  GActionGroup *actions = G_ACTION_GROUP (fixture->device);
  JsonNode *packet;

  valent_test_fixture_connect (fixture, TRUE);

  /* Expect request (thread digest) */
  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.sms.request_conversations");
  json_node_unref (packet);

  g_assert_true (g_action_group_has_action (actions, "sms.messaging"));
  g_assert_true (g_action_group_has_action (actions, "sms.fetch"));

  /* Expect request (thread digest) */
  g_action_group_activate_action (actions, "sms.fetch", NULL);
  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.sms.request_conversations");
  json_node_unref (packet);

  /* Open window */
  g_action_group_activate_action (actions, "sms.messaging", NULL);
}

static void
test_sms_plugin_handle_request (ValentTestFixture *fixture,
                                gconstpointer      user_data)
{
  JsonNode *packet;

  valent_test_fixture_connect (fixture, TRUE);

  /* Expect request (thread digest), then respond */
  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.sms.request_conversations");
  json_node_unref (packet);

  packet = valent_test_fixture_lookup_packet (fixture, "thread-digest");
  valent_test_fixture_handle_packet (fixture, packet);

  /* Expect request (thread 1), then respond */
  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.sms.request_conversation");
  v_assert_packet_cmpint (packet, "threadID", ==, 1);
  json_node_unref (packet);

  packet = valent_test_fixture_lookup_packet (fixture, "thread-1");
  valent_test_fixture_handle_packet (fixture, packet);

  /* Expect request (thread 2), then respond */
  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.sms.request_conversation");
  v_assert_packet_cmpint (packet, "threadID", ==, 2);
  json_node_unref (packet);

  packet = valent_test_fixture_lookup_packet (fixture, "thread-2");
  valent_test_fixture_handle_packet (fixture, packet);
}

static const char *schemas[] = {
  JSON_SCHEMA_DIR"/kdeconnect.sms.attachment_file.json",
  /* JSON_SCHEMA_DIR"/kdeconnect.sms.messages.json", */
  /* JSON_SCHEMA_DIR"/kdeconnect.sms.request.json", */
  JSON_SCHEMA_DIR"/kdeconnect.sms.request_attachment.json",
  JSON_SCHEMA_DIR"/kdeconnect.sms.request_conversation.json",
  JSON_SCHEMA_DIR"/kdeconnect.sms.request_conversations.json",
};

static void
test_sms_plugin_fuzz (ValentTestFixture *fixture,
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
  const char *path = TEST_DATA_DIR"/plugin-sms.json";

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

#ifdef VALENT_TEST_FUZZ
  g_test_add ("/plugins/sms/fuzz",
              ValentTestFixture, path,
              valent_test_fixture_init,
              test_sms_plugin_fuzz,
              valent_test_fixture_clear);
#endif

  return g_test_run ();
}
