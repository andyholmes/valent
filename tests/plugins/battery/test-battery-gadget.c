// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>
#include <valent.h>
#include <libvalent-test.h>


static void
test_battery_plugin_gadget (ValentTestFixture *fixture,
                            gconstpointer      user_data)
{
  PeasEngine *engine;
  PeasExtension *gadget;
  PeasPluginInfo *info;
  ValentDevice *device;
  JsonNode *packet;

  engine = valent_get_plugin_engine ();
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
  valent_test_fixture_connect (fixture, TRUE);

  // NOTE: `ValentBattery` starts with is-present=false so there is no
  //       expectation of a connect-time packet here.
#if 0
  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.battery");
  v_assert_packet_cmpint (packet, "currentCharge", ==, 0);
  v_assert_packet_false (packet, "isCharging");
  v_assert_packet_cmpint (packet, "thresholdEvent", ==, 0);
  json_node_unref (packet);
#endif

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.battery.request");
  v_assert_packet_true (packet, "request");
  json_node_unref (packet);

  /* Switch up the state */
  packet = valent_test_fixture_lookup_packet (fixture, "missing-battery");

  for (unsigned int level = 0; level <= 100; level += 10)
    {
      JsonObject *body = valent_packet_get_body (packet);

      json_object_set_int_member (body, "currentCharge", level);
      json_object_set_boolean_member (body, "isCharging", (level % 20) != 0);
      valent_test_fixture_handle_packet (fixture, packet);
    }

  g_object_unref (gadget);
}

int
main (int   argc,
      char *argv[])
{
  const char *path = "plugin-battery.json";

  valent_test_ui_init (&argc, &argv, NULL);

  g_test_add ("/plugins/battery/gadget",
              ValentTestFixture, path,
              valent_test_fixture_init,
              test_battery_plugin_gadget,
              valent_test_fixture_clear);

  return g_test_run ();
}

