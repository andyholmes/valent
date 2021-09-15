// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>
#include <libvalent-core.h>
#include <libvalent-test.h>

#define DEVICE_PATH "/org/freedesktop/UPower/devices/DisplayDevice"


static void
upower_set_battery (GDBusConnection         *connection,
                    const char              *name,
                    GVariant                *value)
{
  GVariantDict dict;
  GVariant *args;

  g_variant_dict_init (&dict, NULL);
  g_variant_dict_insert_value (&dict, name, value);
  args = g_variant_new ("(o@a{sv})", DEVICE_PATH, g_variant_dict_end (&dict));

  g_dbus_connection_call (connection,
                          "org.freedesktop.UPower",
                          "/org/freedesktop/UPower",
                          "org.freedesktop.DBus.Mock",
                          "SetDeviceProperties",
                          args,
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          NULL,
                          NULL,
                          NULL);
}

static void
test_battery_plugin_actions (ValentTestPluginFixture *fixture,
                             gconstpointer            user_data)
{
  ValentDevice *device;
  GActionGroup *actions;
  g_autoptr (GVariant) state = NULL;
  int level, charging;
  unsigned int time;
  const char *icon_name;

  /* Get the stateful actions */
  device = valent_test_plugin_fixture_get_device (fixture);
  actions = valent_device_get_actions (device);
  g_assert_true (g_action_group_has_action (actions, "battery"));

  /* Local */
  state = g_action_group_get_action_state (actions, "battery");
  g_variant_get (state, "(b&siu)", &charging, &icon_name, &level, &time);

  g_assert_false (charging);
  g_assert_cmpint (level, ==, -1);
  g_assert_cmpuint (time, ==, 0);
}

static void
test_battery_plugin_connect (ValentTestPluginFixture *fixture,
                             gconstpointer            user_data)
{
  JsonNode *packet;

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
}

static void
test_battery_plugin_handle_update (ValentTestPluginFixture *fixture,
                                   gconstpointer            user_data)
{
  ValentDevice *device = valent_test_plugin_fixture_get_device (fixture);
  JsonNode *packet;
  GActionGroup *actions;
  GVariant *state;
  gint level, charging, time;
  const char *icon_name;

  /* Get the stateful actions */
  device = valent_test_plugin_fixture_get_device (fixture);
  actions = valent_device_get_actions (device);
  g_assert_true (g_action_group_has_action (actions, "battery"));

  /* Caution Battery */
  packet = valent_test_plugin_fixture_lookup_packet (fixture, "caution-battery");
  valent_test_plugin_fixture_handle_packet (fixture, packet);

  state = g_action_group_get_action_state (actions, "battery");
  g_variant_get (state, "(b&siu)", &charging, &icon_name, &level, &time);

  g_assert_true (charging);
  g_assert_cmpint (level, ==, 5);
  g_assert_cmpuint (time, !=, 0);

  g_variant_unref (state);

  /* Low Battery */
  packet = valent_test_plugin_fixture_lookup_packet (fixture, "low-battery");
  valent_test_plugin_fixture_handle_packet (fixture, packet);

  state = g_action_group_get_action_state (actions, "battery");
  g_variant_get (state, "(b&siu)", &charging, &icon_name, &level, &time);

  g_assert_true (charging);
  g_assert_cmpint (level, ==, 25);
  g_assert_cmpuint (time, !=, 0);

  g_variant_unref (state);

  /* Good Battery */
  packet = valent_test_plugin_fixture_lookup_packet (fixture, "good-battery");
  valent_test_plugin_fixture_handle_packet (fixture, packet);

  state = g_action_group_get_action_state (actions, "battery");
  g_variant_get (state, "(b&siu)", &charging, &icon_name, &level, &time);

  g_assert_false (charging);
  g_assert_cmpint (level, ==, 50);
  g_assert_cmpuint (time, !=, 0);

  g_variant_unref (state);

  /* Full Battery */
  packet = valent_test_plugin_fixture_lookup_packet (fixture, "full-battery");
  valent_test_plugin_fixture_handle_packet (fixture, packet);

  state = g_action_group_get_action_state (actions, "battery");
  g_variant_get (state, "(b&siu)", &charging, &icon_name, &level, &time);

  g_assert_false (charging);
  g_assert_cmpint (level, ==, 100);
  g_assert_cmpuint (time, !=, 0);

  g_variant_unref (state);
}

static void
test_battery_plugin_handle_request (ValentTestPluginFixture *fixture,
                                    gconstpointer            user_data)
{
  g_autoptr (GDBusConnection) connection = NULL;
  JsonNode *packet;

  connection = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, NULL);

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

  /* Expect updates */
  upower_set_battery (connection, "Percentage", g_variant_new_double (42.0));

  packet = valent_test_plugin_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.battery");
  v_assert_packet_cmpint (packet, "currentCharge", ==, 42);
  v_assert_packet_false (packet, "isCharging");
  v_assert_packet_cmpint (packet, "thresholdEvent", ==, 0);
  json_node_unref (packet);

  upower_set_battery (connection, "State", g_variant_new_uint32 (1));

  packet = valent_test_plugin_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.battery");
  v_assert_packet_cmpint (packet, "currentCharge", ==, 42);
  v_assert_packet_true (packet, "isCharging");
  v_assert_packet_cmpint (packet, "thresholdEvent", ==, 0);
  json_node_unref (packet);

  upower_set_battery (connection, "WarningLevel", g_variant_new_uint32 (3));

  packet = valent_test_plugin_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.battery");
  v_assert_packet_cmpint (packet, "currentCharge", ==, 42);
  v_assert_packet_true (packet, "isCharging");
  v_assert_packet_cmpint (packet, "thresholdEvent", ==, 1);
  json_node_unref (packet);

  /* Respond to a request */
  packet = valent_test_plugin_fixture_lookup_packet (fixture, "request-state");
  valent_test_plugin_fixture_handle_packet (fixture, packet);

  packet = valent_test_plugin_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.battery");
  v_assert_packet_cmpint (packet, "currentCharge", ==, 42);
  v_assert_packet_true (packet, "isCharging");
  v_assert_packet_cmpint (packet, "thresholdEvent", ==, 1);
  json_node_unref (packet);
}

static const char *schemas[] = {
  JSON_SCHEMA_DIR"/kdeconnect.battery.json",
};

static void
test_battery_plugin_fuzz (ValentTestPluginFixture *fixture,
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
  const char *path = TEST_DATA_DIR"/plugin-battery.json";

  g_test_init (&argc, &argv, G_TEST_OPTION_ISOLATE_DIRS, NULL);

  g_test_add ("/plugins/battery/actions",
              ValentTestPluginFixture, path,
              valent_test_plugin_fixture_init,
              test_battery_plugin_actions,
              valent_test_plugin_fixture_clear);

  g_test_add ("/plugins/battery/connect",
              ValentTestPluginFixture, path,
              valent_test_plugin_fixture_init,
              test_battery_plugin_connect,
              valent_test_plugin_fixture_clear);

  g_test_add ("/plugins/battery/handle-update",
              ValentTestPluginFixture, path,
              valent_test_plugin_fixture_init,
              test_battery_plugin_handle_update,
              valent_test_plugin_fixture_clear);

  g_test_add ("/plugins/battery/handle-request",
              ValentTestPluginFixture, path,
              valent_test_plugin_fixture_init,
              test_battery_plugin_handle_request,
              valent_test_plugin_fixture_clear);

#ifdef VALENT_TEST_FUZZ
  g_test_add ("/plugins/battery/fuzz",
              ValentTestPluginFixture, path,
              valent_test_plugin_fixture_init,
              test_battery_plugin_fuzz,
              valent_test_plugin_fixture_clear);
#endif

  return g_test_run ();
}
