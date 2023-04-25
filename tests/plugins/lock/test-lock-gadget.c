// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>
#include <valent.h>
#include <libvalent-test.h>


static void
test_lock_plugin_gadget (ValentTestFixture *fixture,
                         gconstpointer      user_data)
{
  PeasEngine *engine;
  GObject *gadget;
  PeasPluginInfo *info;
  ValentDevice *device;
  JsonNode *packet;

  VALENT_TEST_CHECK ("Plugin can be constructed");
  engine = valent_get_plugin_engine ();
  info = peas_engine_get_plugin_info (engine, "lock");
  gadget = peas_engine_create_extension (engine,
                                         info,
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

  VALENT_TEST_CHECK ("Plugin requests the locked state on connect");
  valent_test_fixture_connect (fixture, TRUE);

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.lock.request");
  v_assert_packet_true (packet, "requestLocked");
  json_node_unref (packet);

  VALENT_TEST_CHECK ("Gadget handles the locked state being changed to TRUE");
  packet = valent_test_fixture_lookup_packet (fixture, "is-locked");
  valent_test_fixture_handle_packet (fixture, packet);

  VALENT_TEST_CHECK ("Gadget handles the locked state being changed to TRUE");
  packet = valent_test_fixture_lookup_packet (fixture, "is-unlocked");
  valent_test_fixture_handle_packet (fixture, packet);

  g_object_unref (gadget);
}

int
main (int   argc,
      char *argv[])
{
  const char *path = "plugin-lock.json";

  valent_test_ui_init (&argc, &argv, NULL);

  g_test_add ("/plugins/lock/gadget",
              ValentTestFixture, path,
              valent_test_fixture_init,
              test_lock_plugin_gadget,
              valent_test_fixture_clear);

  return g_test_run ();
}

