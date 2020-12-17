#include <gio/gio.h>
#include <libvalent-core.h>
#include <libvalent-test.h>

#include "kdeconnect.photo-fuzz.h"
#include "kdeconnect.photo.request-fuzz.h"


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

static void
test_photo_plugin_fuzz (ValentTestPluginFixture *fixture,
                        gconstpointer            user_data)

{
  g_autoptr (JsonParser) parser = NULL;
  JsonNode *packet = NULL;

  valent_test_plugin_fixture_connect (fixture, TRUE);
  g_test_log_set_fatal_handler (valent_test_mute_fuzzing, NULL);

  parser = json_parser_new ();

  for (unsigned int i = 0; i < G_N_ELEMENTS (photo_fuzz); i++)
    {
      json_parser_load_from_data (parser,
                                  photo_fuzz[i].json,
                                  photo_fuzz[i].size,
                                  NULL);
      packet = json_parser_get_root (parser);

      if (VALENT_IS_PACKET (packet))
        valent_test_plugin_fixture_handle_packet (fixture, packet);
    }

  for (unsigned int i = 0; i < G_N_ELEMENTS (photo_request_fuzz); i++)
    {
      json_parser_load_from_data (parser,
                                  photo_request_fuzz[i].json,
                                  photo_request_fuzz[i].size,
                                  NULL);
      packet = json_parser_get_root (parser);

      if (VALENT_IS_PACKET (packet))
        valent_test_plugin_fixture_handle_packet (fixture, packet);
    }
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
