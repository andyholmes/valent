#include <gio/gio.h>
#include <libvalent-core.h>
#include <libvalent-test.h>


static void
clipboard_plugin_fixture_set_up (ValentTestPluginFixture *fixture,
                                 gconstpointer            user_data)
{
  valent_test_plugin_fixture_init (fixture, user_data);
  valent_test_plugin_fixture_init_settings (fixture, "clipboard");
}

static void
clipboard_plugin_fixture_tear_down (ValentTestPluginFixture *fixture,
                                    gconstpointer            user_data)
{
  g_clear_pointer (&fixture->data, g_free);
  valent_test_plugin_fixture_clear (fixture, user_data);
}

static void
get_text_cb (ValentClipboard         *clipboard,
             GAsyncResult            *result,
             ValentTestPluginFixture *fixture)
{
  GError *error = NULL;

  fixture->data = valent_clipboard_get_text_finish (clipboard, result, &error);
  g_assert_no_error (error);

  valent_test_plugin_fixture_quit (fixture);
}

static void
test_clipboard_plugin_connect (ValentTestPluginFixture *fixture,
                               gconstpointer            user_data)
{
  JsonNode *packet;

  g_settings_set_boolean (fixture->settings, "auto-push", TRUE);
  valent_test_plugin_fixture_connect (fixture, TRUE);

  packet = valent_test_plugin_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.clipboard.connect");
  json_node_unref (packet);
}

static void
test_clipboard_plugin_handle_content (ValentTestPluginFixture *fixture,
                                      gconstpointer            user_data)
{
  JsonNode *packet;

  g_settings_set_boolean (fixture->settings, "auto-pull", TRUE);

  /* Regular content */
  packet = valent_test_plugin_fixture_lookup_packet (fixture, "clipboard-content");
  valent_test_plugin_fixture_handle_packet (fixture, packet);

  valent_clipboard_get_text_async (valent_clipboard_get_default (),
                                   NULL,
                                   (GAsyncReadyCallback)get_text_cb,
                                   fixture);
  valent_test_plugin_fixture_run (fixture);

  g_assert_cmpstr (fixture->data, ==, "clipboard-content");
  g_clear_pointer (&fixture->data, g_free);

  /* Connect content */
  packet = valent_test_plugin_fixture_lookup_packet (fixture, "clipboard-connect");
  valent_test_plugin_fixture_handle_packet (fixture, packet);

  valent_clipboard_get_text_async (valent_clipboard_get_default (),
                                   NULL,
                                   (GAsyncReadyCallback)get_text_cb,
                                   fixture);
  valent_test_plugin_fixture_run (fixture);

  g_assert_cmpstr (fixture->data, ==, "clipboard-connect");
  g_clear_pointer (&fixture->data, g_free);
}

static void
test_clipboard_plugin_send_content (ValentTestPluginFixture *fixture,
                                    gconstpointer            user_data)
{
  JsonNode *packet;

  g_settings_set_boolean (fixture->settings, "auto-push", TRUE);
  valent_test_plugin_fixture_connect (fixture, TRUE);

  /* Expect the "connect" packet */
  packet = valent_test_plugin_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.clipboard.connect");
  json_node_unref (packet);

  /* Expect clipboard changes */
  valent_clipboard_set_text (valent_clipboard_get_default (), "send-content");

  packet = valent_test_plugin_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.clipboard");
  v_assert_packet_cmpstr (packet, "content", ==, "send-content");
  json_node_unref (packet);
}

static void
test_clipboard_plugin_actions (ValentTestPluginFixture *fixture,
                               gconstpointer            user_data)
{
  ValentDevice *device;
  GActionGroup *actions;
  JsonNode *packet;

  /* Get the stateful actions */
  g_settings_set_boolean (fixture->settings, "auto-push", FALSE);
  g_settings_set_boolean (fixture->settings, "auto-pull", FALSE);
  valent_test_plugin_fixture_connect (fixture, TRUE);

  device = valent_test_plugin_fixture_get_device (fixture);
  actions = valent_device_get_actions (device);

  g_assert_true (g_action_group_get_action_enabled (actions, "clipboard-pull"));
  g_assert_true (g_action_group_get_action_enabled (actions, "clipboard-push"));

  /* Pull */
  packet = valent_test_plugin_fixture_lookup_packet (fixture, "clipboard-content");
  valent_test_plugin_fixture_handle_packet (fixture, packet);

  g_action_group_activate_action (actions, "clipboard-pull", NULL);
  valent_clipboard_get_text_async (valent_clipboard_get_default (),
                                   NULL,
                                   (GAsyncReadyCallback)get_text_cb,
                                   fixture);
  valent_test_plugin_fixture_run (fixture);

  g_assert_cmpstr (fixture->data, ==, "clipboard-content");
  g_clear_pointer (&fixture->data, g_free);

  /* Push */
  g_action_group_activate_action (actions, "clipboard-push", NULL);

  packet = valent_test_plugin_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.clipboard");
  v_assert_packet_cmpstr (packet, "content", ==, "clipboard-content");
  json_node_unref (packet);
}

static const char *schemas[] = {
  TEST_DATA_DIR"/schemas/kdeconnect.clipboard.json",
  TEST_DATA_DIR"/schemas/kdeconnect.clipboard.connect.json",
};

static void
test_clipboard_plugin_fuzz (ValentTestPluginFixture *fixture,
                            gconstpointer            user_data)

{
  valent_test_plugin_fixture_connect (fixture, TRUE);
  g_test_log_set_fatal_handler (valent_test_mute_fuzzing, NULL);

  for (unsigned int s = 0; s < G_N_ELEMENTS (schemas); s++)
    valent_test_plugin_fixture_schema_fuzz (fixture, schemas[s]);
}

int
main (int   argc,
      char *argv[])
{
  const char *path = TEST_DATA_DIR"/plugin-clipboard.json";

  g_test_init (&argc, &argv, G_TEST_OPTION_ISOLATE_DIRS, NULL);

  g_test_add ("/plugins/clipboard/connect",
              ValentTestPluginFixture, path,
              clipboard_plugin_fixture_set_up,
              test_clipboard_plugin_connect,
              clipboard_plugin_fixture_tear_down);

  g_test_add ("/plugins/clipboard/handle-content",
              ValentTestPluginFixture, path,
              clipboard_plugin_fixture_set_up,
              test_clipboard_plugin_handle_content,
              clipboard_plugin_fixture_tear_down);

  g_test_add ("/plugins/clipboard/send-content",
              ValentTestPluginFixture, path,
              clipboard_plugin_fixture_set_up,
              test_clipboard_plugin_send_content,
              clipboard_plugin_fixture_tear_down);

  g_test_add ("/plugins/clipboard/actions",
              ValentTestPluginFixture, path,
              clipboard_plugin_fixture_set_up,
              test_clipboard_plugin_actions,
              clipboard_plugin_fixture_tear_down);

#ifdef VALENT_TEST_FUZZ
  g_test_add ("/plugins/clipboard/fuzz",
              ValentTestPluginFixture, path,
              valent_test_plugin_fixture_init,
              test_clipboard_plugin_fuzz,
              valent_test_plugin_fixture_clear);
#endif

  return g_test_run ();
}
