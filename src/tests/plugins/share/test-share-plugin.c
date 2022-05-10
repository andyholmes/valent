// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <locale.h>

#include <gio/gio.h>
#include <libvalent-core.h>
#include <libvalent-test.h>


static const char * const test_files[] = {
    "file://"TEST_DATA_DIR"/image.png",
    "file://"TEST_DATA_DIR"/contact.vcf",
    "file://"TEST_DATA_DIR"/contact2.vcf",
    "file://"TEST_DATA_DIR"/contact3.vcf",
};

static const char * const test_uris[] = {
  "mailto:contact@andyholmes.ca",
  "tel:5552368",
  "https://gnome.org",
  "file://"TEST_DATA_DIR"/image.png",
  "file://"TEST_DATA_DIR"/contact.vcf",
  "file://"TEST_DATA_DIR"/contact2.vcf",
  "file://"TEST_DATA_DIR"/contact3.vcf",
};
static guint n_test_uris = G_N_ELEMENTS (test_uris);


static void
test_share_plugin_basic (ValentTestFixture *fixture,
                         gconstpointer      user_data)
{
  GActionGroup *actions = G_ACTION_GROUP (fixture->device);

  g_assert_true (g_action_group_has_action (actions, "share.chooser"));
  g_assert_true (g_action_group_has_action (actions, "share.cancel"));
  g_assert_true (g_action_group_has_action (actions, "share.open"));
  g_assert_true (g_action_group_has_action (actions, "share.text"));
  g_assert_true (g_action_group_has_action (actions, "share.uri"));
  g_assert_true (g_action_group_has_action (actions, "share.uris"));
  g_assert_true (g_action_group_has_action (actions, "share.view"));
}

static void
test_share_plugin_handle_request (ValentTestFixture *fixture,
                                  gconstpointer      user_data)
{
  GError *error = NULL;
  g_autoptr (GFile) file = NULL;
  JsonNode *packet;

  valent_test_fixture_connect (fixture, TRUE);
  file = g_file_new_for_uri ("file://"TEST_DATA_DIR"/image.png");

  /* Receive a file */
  packet = valent_test_fixture_lookup_packet (fixture, "share-file");
  valent_test_fixture_upload (fixture, packet, file, &error);
  g_assert_no_error (error);

  /* Receive a file (Legacy) */
  packet = valent_test_fixture_lookup_packet (fixture, "share-file-legacy");
  valent_test_fixture_upload (fixture, packet, file, &error);
  g_assert_no_error (error);

  /* Receive a file (Open) */
  packet = valent_test_fixture_lookup_packet (fixture, "share-file-open");
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
test_share_plugin_open (ValentTestFixture *fixture,
                        gconstpointer      user_data)
{
  GActionGroup *actions = G_ACTION_GROUP (fixture->device);
  g_autoptr (GFile) file = NULL;
  g_autoptr (GFileInfo) info = NULL;
  goffset size = 0;
  JsonNode *packet;
  GError *error = NULL;

  valent_test_fixture_connect (fixture, TRUE);

  g_assert_true (g_action_group_get_action_enabled (actions, "share.open"));

  /* Expect bogus URIs to be rejected */
  if (g_test_subprocess ())
    {
      g_action_group_activate_action (actions,
                                      "share.open",
                                      g_variant_new_string ("Bogus URI"));
      return;
    }
  g_test_trap_subprocess (NULL, 0, 0);
  g_test_trap_assert_failed ();

  /* Open a URL */
  g_action_group_activate_action (actions,
                                  "share.open",
                                  g_variant_new_string ("tel:5552368"));

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.share.request");
  v_assert_packet_cmpstr (packet, "url", ==, "tel:5552368");
  json_node_unref (packet);

  /* Open a file */
  file = g_file_new_for_uri (test_files[0]);
  info = g_file_query_info (file,
                            G_FILE_ATTRIBUTE_STANDARD_SIZE,
                            G_FILE_QUERY_INFO_NONE,
                            NULL,
                            &error);
  g_assert_no_error (error);
  size = g_file_info_get_size (info);

  g_action_group_activate_action (actions,
                                  "share.open",
                                  g_variant_new_string (test_files[0]));

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.share.request");
  v_assert_packet_cmpstr (packet, "filename", ==, "image.png");
  v_assert_packet_cmpint (packet, "creationTime", >=, 0);
  v_assert_packet_cmpint (packet, "lastModified", >=, 0);
  v_assert_packet_true (packet, "open");
  g_assert_cmpint (valent_packet_get_payload_size (packet), ==, size);

  valent_test_fixture_download (fixture, packet, &error);
  g_assert_no_error (error);

  json_node_unref (packet);
}

static void
test_share_plugin_text (ValentTestFixture *fixture,
                        gconstpointer      user_data)
{
  GActionGroup *actions = G_ACTION_GROUP (fixture->device);
  g_autofree char *text = NULL;
  JsonNode *packet;

  valent_test_fixture_connect (fixture, TRUE);

  g_assert_true (g_action_group_get_action_enabled (actions, "share.text"));

  /* Share text */
  text = g_uuid_string_random ();
  g_action_group_activate_action (actions,
                                  "share.text",
                                  g_variant_new_string (text));

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.share.request");
  v_assert_packet_cmpstr (packet, "text", ==, text);
  json_node_unref (packet);
}

static void
test_share_plugin_uri (ValentTestFixture *fixture,
                       gconstpointer      user_data)
{
  GActionGroup *actions = G_ACTION_GROUP (fixture->device);
  g_autoptr (GFile) file = NULL;
  g_autoptr (GFileInfo) info = NULL;
  goffset size = 0;
  JsonNode *packet;
  GError *error = NULL;

  valent_test_fixture_connect (fixture, TRUE);

  g_assert_true (g_action_group_get_action_enabled (actions, "share.uri"));

  /* Expect bogus URIs to be rejected */
  if (g_test_subprocess ())
    {
      g_action_group_activate_action (actions,
                                      "share.uri",
                                      g_variant_new_string ("Bogus URI"));
      return;
    }
  g_test_trap_subprocess (NULL, 0, 0);
  g_test_trap_assert_failed ();

  /* Share a URL */
  g_action_group_activate_action (actions,
                                  "share.uri",
                                  g_variant_new_string ("https://gnome.org"));

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.share.request");
  v_assert_packet_cmpstr (packet, "url", ==, "https://gnome.org");
  json_node_unref (packet);

  /* Expect file URIs to be converted to uploads */
  file = g_file_new_for_uri (test_files[0]);
  info = g_file_query_info (file,
                            G_FILE_ATTRIBUTE_STANDARD_SIZE,
                            G_FILE_QUERY_INFO_NONE,
                            NULL,
                            &error);
  g_assert_no_error (error);
  size = g_file_info_get_size (info);

  g_action_group_activate_action (actions,
                                  "share.uri",
                                  g_variant_new_string (test_files[0]));

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.share.request.update");
  v_assert_packet_cmpint (packet, "numberOfFiles", ==, 1);
  v_assert_packet_cmpint (packet, "totalPayloadSize", ==, size);
  json_node_unref (packet);

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.share.request");
  v_assert_packet_cmpstr (packet, "filename", ==, "image.png");
  v_assert_packet_cmpint (packet, "creationTime", >=, 0);
  v_assert_packet_cmpint (packet, "lastModified", >=, 0);
  g_assert_cmpint (valent_packet_get_payload_size (packet), ==, size);

  valent_test_fixture_download (fixture, packet, &error);
  g_assert_no_error (error);

  json_node_unref (packet);
}

static void
test_share_plugin_uris (ValentTestFixture *fixture,
                        gconstpointer      user_data)
{
  GActionGroup *actions = G_ACTION_GROUP (fixture->device);
  JsonNode *packet;
  GError *error = NULL;

  valent_test_fixture_connect (fixture, TRUE);

  g_assert_true (g_action_group_get_action_enabled (actions, "share.uris"));

  g_action_group_activate_action (actions,
                                  "share.uris",
                                  g_variant_new_strv (test_uris, n_test_uris));

  /* Expect URIs to be sent as URLs */
  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.share.request");
  v_assert_packet_cmpstr (packet, "url", ==, "mailto:contact@andyholmes.ca");
  json_node_unref (packet);

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.share.request");
  v_assert_packet_cmpstr (packet, "url", ==, "tel:5552368");
  json_node_unref (packet);

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.share.request");
  v_assert_packet_cmpstr (packet, "url", ==, "https://gnome.org");
  json_node_unref (packet);

  /* Expect File URIs to be converted to uploads */
  for (unsigned int i = 0; i < 4; i++)
    {
      packet = valent_test_fixture_expect_packet (fixture);

      while (g_str_equal ("kdeconnect.share.request.update",
                          valent_packet_get_type (packet)))
        {
          v_assert_packet_field (packet, "numberOfFiles");
          v_assert_packet_field (packet, "totalPayloadSize");
          json_node_unref (packet);

          packet = valent_test_fixture_expect_packet (fixture);
        }

      v_assert_packet_type (packet, "kdeconnect.share.request");
      v_assert_packet_field (packet, "filename");
      v_assert_packet_field (packet, "creationTime");
      v_assert_packet_field (packet, "lastModified");
      v_assert_packet_field (packet, "numberOfFiles");
      v_assert_packet_field (packet, "totalPayloadSize");

      valent_test_fixture_download (fixture, packet, &error);
      g_assert_no_error (error);
      json_node_unref (packet);
    }
}

static const char *schemas[] = {
  JSON_SCHEMA_DIR"/kdeconnect.share.request.json",
  JSON_SCHEMA_DIR"/kdeconnect.share.request.update.json",
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

  valent_test_init (&argc, &argv, NULL);

  /* NOTE: This suite will time out if valent_ui_test_init() is used */
  gtk_disable_setlocale ();
  setlocale (LC_ALL, "en_US.UTF-8");
  gtk_init ();

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

  g_test_add ("/plugins/share/open",
              ValentTestFixture, path,
              valent_test_fixture_init,
              test_share_plugin_open,
              valent_test_fixture_clear);

  g_test_add ("/plugins/share/text",
              ValentTestFixture, path,
              valent_test_fixture_init,
              test_share_plugin_text,
              valent_test_fixture_clear);

  g_test_add ("/plugins/share/uri",
              ValentTestFixture, path,
              valent_test_fixture_init,
              test_share_plugin_uri,
              valent_test_fixture_clear);

  g_test_add ("/plugins/share/uris",
              ValentTestFixture, path,
              valent_test_fixture_init,
              test_share_plugin_uris,
              valent_test_fixture_clear);

  g_test_add ("/plugins/share/fuzz",
              ValentTestFixture, path,
              valent_test_fixture_init,
              test_share_plugin_fuzz,
              valent_test_fixture_clear);

  return g_test_run ();
}
