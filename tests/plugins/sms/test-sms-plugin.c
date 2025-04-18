// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>
#include <valent.h>
#include <libvalent-test.h>

static void
test_sms_plugin_basic (ValentTestFixture *fixture,
                       gconstpointer      user_data)
{
  GActionGroup *actions = G_ACTION_GROUP (fixture->device);
  JsonNode *packet;

  VALENT_TEST_CHECK ("Plugin has expected actions");
  g_assert_true (g_action_group_has_action (actions, "sms.sync"));

  valent_test_fixture_connect (fixture, TRUE);

  VALENT_TEST_CHECK ("Plugin requests the threads on connect");
  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.sms.request_conversations");
  json_node_unref (packet);

  VALENT_TEST_CHECK ("Plugin actions are enabled when connected");
  g_assert_true (g_action_group_get_action_enabled (actions, "sms.sync"));

  VALENT_TEST_CHECK ("Plugin action `sms.sync` sends a request for the thread list");
  g_action_group_activate_action (actions, "sms.sync", NULL);
  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.sms.request_conversations");
  json_node_unref (packet);
}

static void
test_sms_plugin_handle_request (ValentTestFixture *fixture,
                                gconstpointer      user_data)
{
  ValentMessages *messages = valent_messages_get_default ();
  g_autoptr (GListModel) adapter = NULL;
  g_autoptr (GListModel) list = NULL;
  g_autoptr (ValentMessage) message = NULL;
  JsonNode *packet;

  adapter = g_list_model_get_item (G_LIST_MODEL (messages), 1);
  g_assert_true (VALENT_IS_MESSAGES_ADAPTER (adapter));

  valent_test_fixture_connect (fixture, TRUE);

  VALENT_TEST_CHECK ("Plugin requests the threads on connect");
  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.sms.request_conversations");
  json_node_unref (packet);

  VALENT_TEST_CHECK ("Plugin handles the latest thread message (1)");
  packet = valent_test_fixture_lookup_packet (fixture, "connect-time-1");
  valent_test_fixture_handle_packet (fixture, packet);

  valent_test_await_signal (adapter, "items-changed");
  list = g_list_model_get_item (adapter, 0);
  g_assert_true (G_IS_LIST_MODEL (list));
  g_assert_cmpuint (g_list_model_get_n_items (list), ==, 0);
  valent_test_await_signal (list, "items-changed");

  VALENT_TEST_CHECK ("Plugin requests the thread (1)");
  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.sms.request_conversation");
  v_assert_packet_cmpint (packet, "threadID", ==, 1);
  json_node_unref (packet);

  VALENT_TEST_CHECK ("Plugin handles the requested thread (1)");
  packet = valent_test_fixture_lookup_packet (fixture, "thread-1");
  valent_test_fixture_handle_packet (fixture, packet);

  valent_test_await_signal (list, "items-changed");
  message = g_list_model_get_item (list, 1);
  g_assert_true (VALENT_IS_MESSAGE (message));
  g_clear_object (&list);
  g_clear_object (&message);

  VALENT_TEST_CHECK ("Plugin handles the latest thread message (2)");
  packet = valent_test_fixture_lookup_packet (fixture, "connect-time-2");
  valent_test_fixture_handle_packet (fixture, packet);

  valent_test_await_signal (adapter, "items-changed");
  list = g_list_model_get_item (adapter, 1);
  g_assert_true (G_IS_LIST_MODEL (list));
  g_assert_cmpuint (g_list_model_get_n_items (list), ==, 0);
  valent_test_await_signal (list, "items-changed");

  VALENT_TEST_CHECK ("Plugin requests the thread (2)");
  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.sms.request_conversation");
  v_assert_packet_cmpint (packet, "threadID", ==, 2);
  json_node_unref (packet);

  VALENT_TEST_CHECK ("Plugin handles the requested thread (2)");
  packet = valent_test_fixture_lookup_packet (fixture, "thread-2");
  valent_test_fixture_handle_packet (fixture, packet);

  valent_test_await_signal (list, "items-changed");
  message = g_list_model_get_item (list, 1);
  g_assert_true (VALENT_IS_MESSAGE (message));
  g_clear_object (&list);
  g_clear_object (&message);
}

static void
test_sms_plugin_handle_attachment (ValentTestFixture *fixture,
                                   gconstpointer      user_data)
{
  ValentMessages *messages = valent_messages_get_default ();
  g_autoptr (GListModel) adapter = NULL;
  g_autoptr (GListModel) list = NULL;
  g_autoptr (ValentMessage) message = NULL;
  JsonNode *packet;
  g_autoptr (GFile) file = NULL;
  GError *error = NULL;

  adapter = g_list_model_get_item (G_LIST_MODEL (messages), 1);
  g_assert_true (VALENT_IS_MESSAGES_ADAPTER (adapter));

  valent_test_fixture_connect (fixture, TRUE);

  VALENT_TEST_CHECK ("Plugin requests the threads on connect");
  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.sms.request_conversations");
  json_node_unref (packet);

  VALENT_TEST_CHECK ("Plugin handles the latest thread message");
  packet = valent_test_fixture_lookup_packet (fixture, "attachment-thread-message");
  valent_test_fixture_handle_packet (fixture, packet);

  valent_test_await_signal (adapter, "items-changed");
  list = g_list_model_get_item (adapter, 0);
  g_assert_true (G_IS_LIST_MODEL (list));
  g_assert_cmpuint (g_list_model_get_n_items (list), ==, 0);
  valent_test_await_signal (list, "items-changed");

  VALENT_TEST_CHECK ("Plugin requests the thread");
  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.sms.request_conversation");
  v_assert_packet_cmpint (packet, "threadID", ==, 42);
  json_node_unref (packet);

  VALENT_TEST_CHECK ("Plugin handles the requested thread");
  packet = valent_test_fixture_lookup_packet (fixture, "attachment-thread");
  valent_test_fixture_handle_packet (fixture, packet);

  valent_test_await_signal (list, "items-changed");
  message = g_list_model_get_item (list, 0);
  g_assert_true (VALENT_IS_MESSAGE (message));
  g_clear_object (&list);
  g_clear_object (&message);

  VALENT_TEST_CHECK ("Plugin requests the attachment");
  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.sms.request_attachment");
  v_assert_packet_cmpint (packet, "part_id", ==, 190);
  v_assert_packet_cmpstr (packet, "unique_identifier", ==, "image.jpg");
  json_node_unref (packet);

  packet = valent_test_fixture_lookup_packet (fixture, "attachment-thread-payload");
  file = g_file_new_for_uri ("resource:///tests/image.jpg");
  valent_test_fixture_upload (fixture, packet, file, &error);
  g_assert_no_error (error);

  valent_test_await_pending ();
}

#if 0
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
#endif

int
main (int   argc,
      char *argv[])
{
  const char *path = "plugin-sms.json";

  valent_test_init (&argc, &argv, NULL);

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

  g_test_add ("/plugins/sms/handle-attachment",
              ValentTestFixture, path,
              valent_test_fixture_init,
              test_sms_plugin_handle_attachment,
              valent_test_fixture_clear);

#if 0
  g_test_add ("/plugins/sms/fuzz",
              ValentTestFixture, path,
              valent_test_fixture_init,
              test_sms_plugin_fuzz,
              valent_test_fixture_clear);
#endif

  return g_test_run ();
}
