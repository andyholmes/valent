// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>
#include <valent.h>
#include <libvalent-test.h>


static void
test_photo_plugin_basic (ValentTestFixture *fixture,
                         gconstpointer      user_data)
{
  GActionGroup *actions = G_ACTION_GROUP (fixture->device);

  g_assert_true (g_action_group_has_action (actions, "photo.request"));
}

static void
test_photo_plugin_send_request (ValentTestFixture *fixture,
                                gconstpointer      user_data)
{
  GActionGroup *actions = G_ACTION_GROUP (fixture->device);
  g_autoptr (GError) error = NULL;
  g_autoptr (GFile) file = NULL;
  JsonNode *packet;

  valent_test_fixture_connect (fixture, TRUE);

  g_assert_true (g_action_group_get_action_enabled (actions, "photo.request"));

  /* Request a photo from the endpoint */
  g_action_group_activate_action (actions, "photo.request", NULL);

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.photo.request");
  json_node_unref (packet);

  /* Upload a photo to the device */
  file = g_file_new_for_uri ("resource:///tests/image.png");
  packet = valent_test_fixture_lookup_packet (fixture, "photo-transfer");
  valent_test_fixture_upload (fixture, packet, file, &error);
  g_assert_no_error (error);
}

static const char *schemas[] = {
  JSON_SCHEMA_DIR"/kdeconnect.photo.json",
  JSON_SCHEMA_DIR"/kdeconnect.photo.request.json",
};

static void
test_photo_plugin_fuzz (ValentTestFixture *fixture,
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
  const char *path = "plugin-photo.json";

  valent_test_init (&argc, &argv, NULL);

  g_test_add ("/plugins/photo/basic",
              ValentTestFixture, path,
              valent_test_fixture_init,
              test_photo_plugin_basic,
              valent_test_fixture_clear);

  g_test_add ("/plugins/photo/send-request",
              ValentTestFixture, path,
              valent_test_fixture_init,
              test_photo_plugin_send_request,
              valent_test_fixture_clear);

  g_test_add ("/plugins/photo/fuzz",
              ValentTestFixture, path,
              valent_test_fixture_init,
              test_photo_plugin_fuzz,
              valent_test_fixture_clear);

  return g_test_run ();
}
