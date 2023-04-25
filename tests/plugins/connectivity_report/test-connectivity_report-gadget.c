// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>
#include <valent.h>
#include <libvalent-test.h>


static void
test_connectivity_report_plugin_gadget (ValentTestFixture *fixture,
                            gconstpointer      user_data)
{
  PeasEngine *engine;
  GObject *gadget;
  PeasPluginInfo *plugin_info;
  ValentDevice *device;
  JsonNode *packet;

  VALENT_TEST_CHECK ("Plugin can be constructed");
  engine = valent_get_plugin_engine ();
  plugin_info = peas_engine_get_plugin_info (engine, "connectivity_report");
  gadget = peas_engine_create_extension (engine,
                                         plugin_info,
                                         VALENT_TYPE_DEVICE_GADGET,
                                         "device", fixture->device,
                                         NULL);
  g_object_ref_sink (gadget);

  VALENT_TEST_CHECK ("GObject properties function correctly");
  g_object_get (gadget,
                "device", &device,
                NULL);
  g_assert_true (fixture->device == device);
  g_object_unref (device);

  VALENT_TEST_CHECK ("Plugin requests the connectivity status on connect");
  valent_test_fixture_connect (fixture, TRUE);

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.connectivity_report.request");
  json_node_unref (packet);

  VALENT_TEST_CHECK ("Plugin handles the \"modemless\" state");
  packet = valent_test_fixture_lookup_packet (fixture, "modemless-report");
  valent_test_fixture_handle_packet (fixture, packet);

  VALENT_TEST_CHECK ("Plugin handles the \"modemless\" state");
  packet = valent_test_fixture_lookup_packet (fixture, "offline-report");
  valent_test_fixture_handle_packet (fixture, packet);

  VALENT_TEST_CHECK ("Plugin handles the \"none\" state");
  packet = valent_test_fixture_lookup_packet (fixture, "none-report");
  valent_test_fixture_handle_packet (fixture, packet);

  VALENT_TEST_CHECK ("Plugin handles the \"weak\" state");
  packet = valent_test_fixture_lookup_packet (fixture, "weak-report");
  valent_test_fixture_handle_packet (fixture, packet);

  VALENT_TEST_CHECK ("Plugin handles the \"ok\" state");
  packet = valent_test_fixture_lookup_packet (fixture, "ok-report");
  valent_test_fixture_handle_packet (fixture, packet);

  VALENT_TEST_CHECK ("Plugin handles the \"good\" state");
  packet = valent_test_fixture_lookup_packet (fixture, "good-report");
  valent_test_fixture_handle_packet (fixture, packet);

  VALENT_TEST_CHECK ("Plugin handles the \"excellent\" state");
  packet = valent_test_fixture_lookup_packet (fixture, "excellent-report");
  valent_test_fixture_handle_packet (fixture, packet);

  VALENT_TEST_CHECK ("Plugin handles other states");
  packet = valent_test_fixture_lookup_packet (fixture, "extra1-report");
  valent_test_fixture_handle_packet (fixture, packet);

  VALENT_TEST_CHECK ("Plugin handles other states");
  packet = valent_test_fixture_lookup_packet (fixture, "extra2-report");
  valent_test_fixture_handle_packet (fixture, packet);

  g_object_unref (gadget);
}

int
main (int   argc,
      char *argv[])
{
  const char *path = "plugin-connectivity_report.json";

  valent_test_ui_init (&argc, &argv, NULL);

  g_test_add ("/plugins/connectivity_report/gadget",
              ValentTestFixture, path,
              valent_test_fixture_init,
              test_connectivity_report_plugin_gadget,
              valent_test_fixture_clear);

  return g_test_run ();
}

