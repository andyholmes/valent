#include <gio/gio.h>
#include <libvalent-core.h>
#include <libvalent-test.h>
#include <libvalent-ui.h>


static void
test_battery_plugin_gadget (ValentTestPluginFixture *fixture,
                            gconstpointer            user_data)
{
  PeasEngine *engine;
  PeasExtension *gadget;
  PeasPluginInfo *info;
  ValentDevice *device;
  JsonNode *packet;

  engine = valent_get_engine ();
  info = peas_engine_get_plugin_info (engine, "battery");
  gadget = peas_engine_create_extension (engine,
                                         info,
                                         VALENT_TYPE_DEVICE_GADGET,
                                         "device", fixture->device,
                                         NULL);
  g_object_ref_sink (gadget);

  /* Properties */
  g_object_get (gadget,
                "device", &device,
                NULL);
  g_assert_true (fixture->device == device);
  g_object_unref (device);

  /* Expect connect packets */
  valent_test_plugin_fixture_connect (fixture, TRUE);

  packet = valent_test_plugin_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.battery");
  v_assert_packet_cmpint (packet, "currentCharge", ==, -1);
  v_assert_packet_false (packet, "isCharging");
  v_assert_packet_cmpint (packet, "thresholdEvent", ==, 0);
  json_node_unref (packet);

  packet = valent_test_plugin_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.battery.request");
  v_assert_packet_true (packet, "request");
  json_node_unref (packet);

  /* Switch up the state */
  packet = valent_test_plugin_fixture_lookup_packet (fixture, "low-battery");
  valent_test_plugin_fixture_handle_packet (fixture, packet);

  packet = valent_test_plugin_fixture_lookup_packet (fixture, "full-battery");
  valent_test_plugin_fixture_handle_packet (fixture, packet);

  g_object_unref (gadget);
}

int
main (int   argc,
      char *argv[])
{
  const char *path = TEST_DATA_DIR"/plugin-battery.json";

  valent_test_ui_init (&argc, &argv, NULL);

  g_test_add ("/plugins/battery/gadget",
              ValentTestPluginFixture, path,
              valent_test_plugin_fixture_init,
              test_battery_plugin_gadget,
              valent_test_plugin_fixture_clear);

  return g_test_run ();
}

