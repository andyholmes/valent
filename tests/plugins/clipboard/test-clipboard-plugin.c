// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>
#include <valent.h>
#include <libvalent-test.h>


static void
valent_clipboard_read_text_cb (ValentClipboard *clipboard,
                               GAsyncResult    *result,
                               gpointer        *text)
{
  GError *error = NULL;

  if (text != NULL)
    *text = valent_clipboard_read_text_finish (clipboard, result, &error);

  g_assert_no_error (error);
}

static void
test_clipboard_plugin_connect (ValentTestFixture *fixture,
                               gconstpointer      user_data)
{
  JsonNode *packet;

  g_settings_set_boolean (fixture->settings, "auto-pull", TRUE);
  g_settings_set_boolean (fixture->settings, "auto-push", TRUE);

  VALENT_TEST_CHECK ("Plugin sends clipboard content at connect time");
  valent_test_fixture_connect (fixture);

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.clipboard.connect");
  json_node_unref (packet);
}

static void
test_clipboard_plugin_handle_content (ValentTestFixture *fixture,
                                      gconstpointer      user_data)
{
  JsonNode *packet;
  char *content = NULL;
  gboolean watch = FALSE;

  valent_test_watch_signal (valent_clipboard_get_default (), "changed", &watch);

  g_settings_set_boolean (fixture->settings, "auto-pull", TRUE);
  g_settings_set_boolean (fixture->settings, "auto-push", TRUE);

  VALENT_TEST_CHECK ("Plugin sends clipboard content at connect time");
  valent_test_fixture_connect (fixture);

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.clipboard.connect");
  json_node_unref (packet);

  VALENT_TEST_CHECK ("Plugin copies connect-time content to the local clipboard");
  packet = valent_test_fixture_lookup_packet (fixture, "clipboard-connect");
  valent_test_fixture_handle_packet (fixture, packet);
  valent_test_await_boolean (&watch);

  valent_clipboard_read_text (valent_clipboard_get_default (),
                              NULL,
                              (GAsyncReadyCallback)valent_clipboard_read_text_cb,
                              &content);
  valent_test_await_pointer (&content);

  g_assert_cmpstr (content, ==, "clipboard-connect");
  g_clear_pointer (&content, g_free);

  VALENT_TEST_CHECK ("Plugin copies remote content to the local clipboard");
  packet = valent_test_fixture_lookup_packet (fixture, "clipboard-content");
  valent_test_fixture_handle_packet (fixture, packet);
  valent_test_await_boolean (&watch);

  valent_clipboard_read_text (valent_clipboard_get_default (),
                              NULL,
                              (GAsyncReadyCallback)valent_clipboard_read_text_cb,
                              &content);
  valent_test_await_pointer (&content);

  g_assert_cmpstr (content, ==, "clipboard-content");
  g_clear_pointer (&content, g_free);

  VALENT_TEST_CHECK ("Plugin ignores connect-time content that is outdated");
  packet = valent_test_fixture_lookup_packet (fixture, "clipboard-connect");
  json_object_set_int_member (valent_packet_get_body (packet), "timestamp", 0);
  json_object_set_string_member (valent_packet_get_body (packet), "content", "old");
  valent_test_fixture_handle_packet (fixture, packet);
  valent_test_await_pending ();

  valent_clipboard_read_text (valent_clipboard_get_default (),
                              NULL,
                              (GAsyncReadyCallback)valent_clipboard_read_text_cb,
                              &content);
  valent_test_await_pointer (&content);

  g_assert_cmpstr (content, ==, "clipboard-content");
  g_clear_pointer (&content, g_free);

  valent_test_watch_clear (valent_clipboard_get_default (), &watch);
}

static void
test_clipboard_plugin_send_content (ValentTestFixture *fixture,
                                    gconstpointer      user_data)
{
  JsonNode *packet;

  g_settings_set_boolean (fixture->settings, "auto-pull", TRUE);
  g_settings_set_boolean (fixture->settings, "auto-push", TRUE);

  VALENT_TEST_CHECK ("Plugin sends clipboard content at connect time");
  valent_test_fixture_connect (fixture);

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.clipboard.connect");
  json_node_unref (packet);

  VALENT_TEST_CHECK ("Plugin sends clipboard content when it changes");
  valent_clipboard_write_text (valent_clipboard_get_default (),
                               "send-content",
                               NULL,
                               NULL,
                               NULL);

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.clipboard");
  v_assert_packet_cmpstr (packet, "content", ==, "send-content");
  json_node_unref (packet);
}

static void
test_clipboard_plugin_actions (ValentTestFixture *fixture,
                               gconstpointer      user_data)
{
  GActionGroup *actions = G_ACTION_GROUP (fixture->device);
  JsonNode *packet;
  char *content = NULL;
  gboolean watch = FALSE;

  valent_test_watch_signal (valent_clipboard_get_default (), "changed", &watch);

  /* NOTE: no connect-time packets with `auto-push` disabled */
  g_settings_set_boolean (fixture->settings, "auto-push", FALSE);
  g_settings_set_boolean (fixture->settings, "auto-pull", FALSE);

  VALENT_TEST_CHECK ("Plugin has expected actions disabled when disconnected");
  g_assert_false (g_action_group_get_action_enabled (actions, "clipboard.pull"));
  g_assert_false (g_action_group_get_action_enabled (actions, "clipboard.push"));

  VALENT_TEST_CHECK ("Plugin has expected actions enabled when connected");
  valent_test_fixture_connect (fixture);
  g_assert_true (g_action_group_get_action_enabled (actions, "clipboard.pull"));
  g_assert_true (g_action_group_get_action_enabled (actions, "clipboard.push"));

  VALENT_TEST_CHECK ("Plugin action `clipboard.pull` copies content to the clipboard");
  packet = valent_test_fixture_lookup_packet (fixture, "clipboard-content");
  valent_test_fixture_handle_packet (fixture, packet);
  valent_test_await_timeout (1);

  g_action_group_activate_action (actions, "clipboard.pull", NULL);
  valent_test_await_boolean (&watch);

  valent_clipboard_read_text (valent_clipboard_get_default (),
                              NULL,
                              (GAsyncReadyCallback)valent_clipboard_read_text_cb,
                              &content);
  valent_test_await_pointer (&content);

  g_assert_cmpstr (content, ==, "clipboard-content");
  g_clear_pointer (&content, g_free);

  VALENT_TEST_CHECK ("Plugin action `clipboard.push` sends content to the device");
  valent_clipboard_write_text (valent_clipboard_get_default (),
                               "push-content",
                               NULL,
                               NULL,
                               NULL);
  valent_test_await_boolean (&watch);

  g_action_group_activate_action (actions, "clipboard.push", NULL);
  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.clipboard");
  v_assert_packet_cmpstr (packet, "content", ==, "push-content");
  json_node_unref (packet);
}

static const char *schemas[] = {
  "/tests/kdeconnect.clipboard.json",
  "/tests/kdeconnect.clipboard.connect.json",
};

static void
test_clipboard_plugin_fuzz (ValentTestFixture *fixture,
                            gconstpointer      user_data)

{
  valent_test_fixture_connect (fixture);
  g_test_log_set_fatal_handler (valent_test_mute_fuzzing, NULL);

  for (size_t s = 0; s < G_N_ELEMENTS (schemas); s++)
    valent_test_fixture_schema_fuzz (fixture, schemas[s]);
}

int
main (int   argc,
      char *argv[])
{
  const char *path = "plugin-clipboard.json";

  valent_test_init (&argc, &argv, NULL);

  g_test_add ("/plugins/clipboard/connect",
              ValentTestFixture, path,
              valent_test_fixture_init,
              test_clipboard_plugin_connect,
              valent_test_fixture_clear);

  g_test_add ("/plugins/clipboard/handle-content",
              ValentTestFixture, path,
              valent_test_fixture_init,
              test_clipboard_plugin_handle_content,
              valent_test_fixture_clear);

  g_test_add ("/plugins/clipboard/send-content",
              ValentTestFixture, path,
              valent_test_fixture_init,
              test_clipboard_plugin_send_content,
              valent_test_fixture_clear);

  g_test_add ("/plugins/clipboard/actions",
              ValentTestFixture, path,
              valent_test_fixture_init,
              test_clipboard_plugin_actions,
              valent_test_fixture_clear);

  g_test_add ("/plugins/clipboard/fuzz",
              ValentTestFixture, path,
              valent_test_fixture_init,
              test_clipboard_plugin_fuzz,
              valent_test_fixture_clear);

  return g_test_run ();
}
