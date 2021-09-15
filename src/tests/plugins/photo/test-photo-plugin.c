// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>
#include <libvalent-core.h>
#include <libvalent-test.h>


static void
test_photo_plugin_basic (ValentTestPluginFixture *fixture,
                         gconstpointer            user_data)
{
  ValentDevice *device;
  GActionGroup *actions;

  device = valent_test_plugin_fixture_get_device (fixture);
  actions = valent_device_get_actions (device);
  g_assert_true (g_action_group_has_action (actions, "photo"));
}

static void
test_photo_plugin_send_request (ValentTestPluginFixture *fixture,
                                gconstpointer            user_data)
{
  g_autoptr (GError) error = NULL;
  g_autoptr (GFile) file = NULL;
  ValentDevice *device;
  GActionGroup *actions;
  JsonNode *packet;

  valent_test_plugin_fixture_connect (fixture, TRUE);

  device = valent_test_plugin_fixture_get_device (fixture);
  actions = valent_device_get_actions (device);
  g_assert_true (g_action_group_get_action_enabled (actions, "photo"));

  /* Request a photo from the endpoint */
  g_action_group_activate_action (actions, "photo", NULL);

  packet = valent_test_plugin_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.photo.request");
  json_node_unref (packet);

  /* Upload a photo to the device */
  file = g_file_new_for_uri ("file://"TEST_DATA_DIR"image.png");
  packet = valent_test_plugin_fixture_lookup_packet (fixture, "photo-transfer");
  valent_test_plugin_fixture_upload (fixture, packet, file, &error);
  g_assert_no_error (error);
}

static const char *schemas[] = {
  JSON_SCHEMA_DIR"/kdeconnect.photo.json",
  JSON_SCHEMA_DIR"/kdeconnect.photo.request.json",
};

static void
test_photo_plugin_fuzz (ValentTestPluginFixture *fixture,
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
  const char *path = TEST_DATA_DIR"/plugin-photo.json";

  g_test_init (&argc, &argv, G_TEST_OPTION_ISOLATE_DIRS, NULL);

  g_test_add ("/plugins/photo/basic",
              ValentTestPluginFixture, path,
              valent_test_plugin_fixture_init,
              test_photo_plugin_basic,
              valent_test_plugin_fixture_clear);

  g_test_add ("/plugins/photo/send-request",
              ValentTestPluginFixture, path,
              valent_test_plugin_fixture_init,
              test_photo_plugin_send_request,
              valent_test_plugin_fixture_clear);

#ifdef VALENT_TEST_FUZZ
  g_test_add ("/plugins/photo/fuzz",
              ValentTestPluginFixture, path,
              valent_test_plugin_fixture_init,
              test_photo_plugin_fuzz,
              valent_test_plugin_fixture_clear);
#endif

  return g_test_run ();
}
