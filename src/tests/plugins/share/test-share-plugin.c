// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>
#include <gtk/gtk.h>
#include <libvalent-core.h>
#include <libvalent-test.h>


static void
test_share_plugin_basic (ValentTestFixture *fixture,
                         gconstpointer      user_data)
{
  GActionGroup *actions = G_ACTION_GROUP (fixture->device);

  g_assert_true (g_action_group_has_action (actions, "share.share"));
  g_assert_true (g_action_group_has_action (actions, "share.cancel"));
  g_assert_true (g_action_group_has_action (actions, "share.files"));
  g_assert_true (g_action_group_has_action (actions, "share.open"));
  g_assert_true (g_action_group_has_action (actions, "share.text"));
  g_assert_true (g_action_group_has_action (actions, "share.url"));
}

static void
test_share_plugin_handle_request (ValentTestFixture *fixture,
                                  gconstpointer      user_data)
{
  GError *error = NULL;
  g_autoptr (GFile) file = NULL;
  JsonNode *packet;

  valent_test_fixture_connect (fixture, TRUE);

  /* Receive a file */
  file = g_file_new_for_uri ("file://"TEST_DATA_DIR"image.png");
  packet = valent_test_fixture_lookup_packet (fixture, "share-file");
  valent_test_fixture_upload (fixture, packet, file, &error);
  g_assert_no_error (error);

  /* Receive text */
  packet = valent_test_fixture_lookup_packet (fixture, "share-text");
  valent_test_fixture_handle_packet (fixture, packet);

  /* Receive a URL */
  packet = valent_test_fixture_lookup_packet (fixture, "share-url");
  valent_test_fixture_handle_packet (fixture, packet);

  while (g_main_context_iteration (NULL, FALSE))
    continue;
}

static void
test_share_plugin_send_request (ValentTestFixture *fixture,
                                gconstpointer      user_data)
{
  GActionGroup *actions = G_ACTION_GROUP (fixture->device);
  JsonNode *packet;
  const char * const files[] = { "file://"TEST_DATA_DIR"image.png" };
  GError *error = NULL;

  valent_test_fixture_connect (fixture, TRUE);

  g_assert_true (g_action_group_get_action_enabled (actions, "share.files"));
  g_assert_true (g_action_group_get_action_enabled (actions, "share.text"));
  g_assert_true (g_action_group_get_action_enabled (actions, "share.url"));

  /* Share a file */
  g_action_group_activate_action (actions,
                                  "share.files",
                                  g_variant_new_strv (files, 1));

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.share.request");
  v_assert_packet_field (packet, "filename");

  valent_test_fixture_download (fixture, packet, &error);
  g_assert_no_error (error);

  json_node_unref (packet);

  /* Share text */
  g_action_group_activate_action (actions,
                                  "share.text",
                                  g_variant_new_string ("Test"));

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.share.request");
  v_assert_packet_cmpstr (packet, "text", ==, "Test");
  json_node_unref (packet);

  /* Share a URL */
  g_action_group_activate_action (actions,
                                  "share.url",
                                  g_variant_new_string ("https://www.andyholmes.ca"));

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.share.request");
  v_assert_packet_cmpstr (packet, "url", ==, "https://www.andyholmes.ca");
  json_node_unref (packet);
}

static const char *schemas[] = {
  JSON_SCHEMA_DIR"/kdeconnect.share.request.json",
};

static void
test_share_plugin_fuzz (ValentTestFixture *fixture,
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
  const char *path = TEST_DATA_DIR"/plugin-share.json";

  g_test_init (&argc, &argv, G_TEST_OPTION_ISOLATE_DIRS, NULL);

  g_test_add ("/plugins/share/basic",
              ValentTestFixture, path,
              valent_test_fixture_init,
              test_share_plugin_basic,
              valent_test_fixture_clear);

  g_test_add ("/plugins/share/handle-request",
              ValentTestFixture, path,
              valent_test_fixture_init,
              test_share_plugin_handle_request,
              valent_test_fixture_clear);

  g_test_add ("/plugins/share/send-request",
              ValentTestFixture, path,
              valent_test_fixture_init,
              test_share_plugin_send_request,
              valent_test_fixture_clear);

#ifdef VALENT_TEST_FUZZ
  g_test_add ("/plugins/share/fuzz",
              ValentTestFixture, path,
              valent_test_fixture_init,
              test_share_plugin_fuzz,
              valent_test_fixture_clear);
#endif

  return g_test_run ();
}
