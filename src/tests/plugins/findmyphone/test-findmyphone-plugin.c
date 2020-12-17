#include <gio/gio.h>
#include <gtk/gtk.h>
#include <gst/gst.h>
#include <libvalent-core.h>
#include <libvalent-test.h>

#include "kdeconnect.findmyphone.request-fuzz.h"


static void
test_findmyphone_plugin_basic (ValentTestPluginFixture *fixture,
                               gconstpointer            user_data)
{
  ValentDevice *device;
  GActionGroup *actions;

  device = valent_test_plugin_fixture_get_device (fixture);
  actions = valent_device_get_actions (device);
  g_assert_true (g_action_group_has_action (actions, "ring"));
}

static gboolean
on_ringing_stop (gpointer data)
{
  ValentTestPluginFixture *fixture = data;

  valent_test_plugin_fixture_quit (fixture);

  return G_SOURCE_REMOVE;
}

static void
test_findmyphone_plugin_handle_request (ValentTestPluginFixture *fixture,
                                        gconstpointer            user_data)
{
  JsonNode *packet;

  packet = valent_test_plugin_fixture_lookup_packet (fixture, "ring-request");

  /* Start ringing */
  valent_test_plugin_fixture_handle_packet (fixture, packet);

  g_timeout_add_seconds (1, on_ringing_stop, fixture);
  valent_test_plugin_fixture_run (fixture);

  /* Stop ringing */
  valent_test_plugin_fixture_handle_packet (fixture, packet);
}

static void
test_findmyphone_plugin_send_request (ValentTestPluginFixture *fixture,
                                      gconstpointer            user_data)
{
  ValentDevice *device;
  JsonNode *packet;
  GActionGroup *actions;

  valent_test_plugin_fixture_connect (fixture, TRUE);

  device = valent_test_plugin_fixture_get_device (fixture);
  actions = valent_device_get_actions (device);
  g_assert_true (g_action_group_get_action_enabled (actions, "ring"));

  g_action_group_activate_action (actions, "ring", NULL);

  packet = valent_test_plugin_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.findmyphone.request");
  json_node_unref (packet);
}

static void
test_findmyphone_plugin_fuzz (ValentTestPluginFixture *fixture,
                              gconstpointer            user_data)

{
  g_autoptr (JsonParser) parser = NULL;
  JsonNode *packet = NULL;

  valent_test_plugin_fixture_connect (fixture, TRUE);
  g_test_log_set_fatal_handler (valent_test_mute_fuzzing, NULL);

  parser = json_parser_new ();

  for (unsigned int i = 0; i < G_N_ELEMENTS (findmyphone_request_fuzz); i++)
    {
      json_parser_load_from_data (parser,
                                  findmyphone_request_fuzz[i].json,
                                  findmyphone_request_fuzz[i].size,
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
  const char *path = TEST_DATA_DIR"/plugin-findmyphone.json";

  g_test_init (&argc, &argv, G_TEST_OPTION_ISOLATE_DIRS, NULL);

  g_test_add ("/plugins/findmyphone/basic",
              ValentTestPluginFixture, path,
              valent_test_plugin_fixture_init,
              test_findmyphone_plugin_basic,
              valent_test_plugin_fixture_clear);

  g_test_add ("/plugins/findmyphone/handle-request",
              ValentTestPluginFixture, path,
              valent_test_plugin_fixture_init,
              test_findmyphone_plugin_handle_request,
              valent_test_plugin_fixture_clear);

  g_test_add ("/plugins/findmyphone/send-request",
              ValentTestPluginFixture, path,
              valent_test_plugin_fixture_init,
              test_findmyphone_plugin_send_request,
              valent_test_plugin_fixture_clear);

#ifdef VALENT_TEST_FUZZ
  g_test_add ("/plugins/findmyphone/fuzz",
              ValentTestPluginFixture, path,
              valent_test_plugin_fixture_init,
              test_findmyphone_plugin_fuzz,
              valent_test_plugin_fixture_clear);
#endif

  if (gst_is_initialized ())
    gst_deinit ();

  return g_test_run ();
}
