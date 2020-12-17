#include <gio/gio.h>
#include <libvalent-core.h>
#include <libvalent-test.h>


static void
test_telephony_plugin_basic (ValentTestPluginFixture *fixture,
                             gconstpointer            user_data)
{
  ValentDevice *device;
  GActionGroup *actions;

  device = valent_test_plugin_fixture_get_device (fixture);
  actions = valent_device_get_actions (device);

  g_assert_true (g_action_group_has_action (actions, "mute-call"));
}

static void
test_telephony_plugin_handle_event (ValentTestPluginFixture *fixture,
                                    gconstpointer            user_data)
{
  JsonNode *packet;

  valent_test_plugin_fixture_connect (fixture, TRUE);

  /* Receive a ringing event */
  packet = valent_test_plugin_fixture_lookup_packet (fixture, "ringing");
  valent_test_plugin_fixture_handle_packet (fixture, packet);

  /* Cancel ringing */
  packet = valent_test_plugin_fixture_lookup_packet (fixture, "ringing-cancel");
  valent_test_plugin_fixture_handle_packet (fixture, packet);

  /* Receive a ringing->telephony event chain */
  packet = valent_test_plugin_fixture_lookup_packet (fixture, "ringing");
  valent_test_plugin_fixture_handle_packet (fixture, packet);

  packet = valent_test_plugin_fixture_lookup_packet (fixture, "talking");
  valent_test_plugin_fixture_handle_packet (fixture, packet);

  /* Cancel talking */
  packet = valent_test_plugin_fixture_lookup_packet (fixture, "talking-cancel");
  valent_test_plugin_fixture_handle_packet (fixture, packet);
}

static void
test_telephony_plugin_mute_call (ValentTestPluginFixture *fixture,
                                 gconstpointer            user_data)
{
  ValentDevice *device;
  GActionGroup *actions;
  JsonNode *packet;

  device = valent_test_plugin_fixture_get_device (fixture);
  actions = valent_device_get_actions (device);

  valent_test_plugin_fixture_connect (fixture, TRUE);

  /* Receive a ringing event */
  packet = valent_test_plugin_fixture_lookup_packet (fixture, "ringing");
  valent_test_plugin_fixture_handle_packet (fixture, packet);

  /* Mute the call */
  g_action_group_activate_action (actions, "mute-call", NULL);

  /* Cancel ringing */
  packet = valent_test_plugin_fixture_lookup_packet (fixture, "ringing-cancel");
  valent_test_plugin_fixture_handle_packet (fixture, packet);
}

int
main (int   argc,
      char *argv[])
{
  const char *path = TEST_DATA_DIR"/plugin-telephony.json";

  g_test_init (&argc, &argv, G_TEST_OPTION_ISOLATE_DIRS, NULL);

  g_test_add ("/plugins/telephony/basic",
              ValentTestPluginFixture, path,
              valent_test_plugin_fixture_init,
              test_telephony_plugin_basic,
              valent_test_plugin_fixture_clear);

  g_test_add ("/plugins/telephony/handle-event",
              ValentTestPluginFixture, path,
              valent_test_plugin_fixture_init,
              test_telephony_plugin_handle_event,
              valent_test_plugin_fixture_clear);

  g_test_add ("/plugins/telephony/mute-call",
              ValentTestPluginFixture, path,
              valent_test_plugin_fixture_init,
              test_telephony_plugin_mute_call,
              valent_test_plugin_fixture_clear);

  return g_test_run ();
}
