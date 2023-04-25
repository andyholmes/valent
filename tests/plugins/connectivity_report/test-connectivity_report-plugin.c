// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>
#include <valent.h>
#include <libvalent-test.h>

#define MODEM_PATH  "/org/freedesktop/ModemManager1/Modem/0"
#define MODEM_IFACE "org.freedesktop.ModemManager1.Modem"


static void
dbusmock_modemmanager_cb (GDBusConnection *connection,
                          GAsyncResult    *result,
                          gpointer         user_data)
{
  g_autoptr (GVariant) reply = NULL;
  GError *error = NULL;

  reply = g_dbus_connection_call_finish (connection, result, &error);
  g_assert_no_error (error);
  g_assert_nonnull (reply);
}

static void
dbusmock_modemmanager (GDBusConnection *connection,
                       const char      *method,
                       unsigned int     index)
{
  g_dbus_connection_call (connection,
                          "org.freedesktop.ModemManager1",
                          "/org/freedesktop/ModemManager1",
                          "org.freedesktop.DBus.Mock",
                          method,
                          g_variant_new ("(u)", index),
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          NULL,
                          (GAsyncReadyCallback)dbusmock_modemmanager_cb,
                          NULL);
}

static void
test_connectivity_report_plugin_actions (ValentTestFixture *fixture,
                                         gconstpointer      user_data)
{
  GActionGroup *actions = G_ACTION_GROUP (fixture->device);
  g_autoptr (GVariant) state = NULL;

  VALENT_TEST_CHECK ("Plugin has expected actions");
  g_assert_true (g_action_group_has_action (actions, "connectivity_report.state"));

  VALENT_TEST_CHECK ("Plugin action `connectivity_report.state` is enabled");
  g_assert_false (g_action_group_get_action_enabled (actions, "connectivity_report.state"));

  VALENT_TEST_CHECK ("Plugin action `connectivity_report.state` has expected signature");
  state = g_action_group_get_action_state (actions, "connectivity_report.state");
  g_assert_true (g_variant_is_of_type (state, G_VARIANT_TYPE_VARDICT));

  VALENT_TEST_CHECK ("Plugin action `connectivity_report.state` has expected value");
  g_assert_cmpuint (g_variant_n_children (state), ==, 0);
}

static void
test_connectivity_report_plugin_connect (ValentTestFixture *fixture,
                                         gconstpointer      user_data)
{
  JsonNode *packet;

  VALENT_TEST_CHECK ("Plugin requests the connectivity status on connect");
  valent_test_fixture_connect (fixture, TRUE);

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.connectivity_report.request");
  json_node_unref (packet);
}

static void
test_connectivity_report_plugin_handle_update (ValentTestFixture *fixture,
                                               gconstpointer      user_data)
{
  GActionGroup *actions = G_ACTION_GROUP (fixture->device);
  JsonNode *packet;
  GVariant *state;
  GVariant *signal_strengths;
  GVariant *signal_info;
  const char *icon_name;
  const char *network_type;
  int64_t signal_strength;

  /* Setup GSettings */
  g_settings_set_boolean (fixture->settings, "offline-notification", TRUE);

  /* Modem is in the default state so the action should be disabled */
  g_assert_false (g_action_group_get_action_enabled (actions, "connectivity_report.state"));

  VALENT_TEST_CHECK ("Plugin handles the \"modemless\" state");
  packet = valent_test_fixture_lookup_packet (fixture, "modemless-report");
  valent_test_fixture_handle_packet (fixture, packet);

  g_assert_false (g_action_group_get_action_enabled (actions, "connectivity_report.state"));
  state = g_action_group_get_action_state (actions, "connectivity_report.state");

  g_variant_lookup (state, "signal-strengths", "@a{sv}", &signal_strengths);
  g_assert_cmpuint (g_variant_n_children (signal_strengths), ==, 0);

  g_clear_pointer (&signal_strengths, g_variant_unref);
  g_clear_pointer (&state, g_variant_unref);

  VALENT_TEST_CHECK ("Plugin handles the \"offline\" state");
  packet = valent_test_fixture_lookup_packet (fixture, "offline-report");
  valent_test_fixture_handle_packet (fixture, packet);

  g_assert_true (g_action_group_get_action_enabled (actions, "connectivity_report.state"));
  state = g_action_group_get_action_state (actions, "connectivity_report.state");

  g_variant_lookup (state, "signal-strengths", "@a{sv}", &signal_strengths);
  g_variant_lookup (signal_strengths, "1", "@a{sv}", &signal_info);

  g_assert_true (g_variant_lookup (signal_info, "network-type", "&s", &network_type));
  g_assert_true (g_variant_lookup (signal_info, "signal-strength", "x", &signal_strength));
  g_assert_true (g_variant_lookup (signal_info, "icon-name", "&s", &icon_name));

  g_assert_cmpstr (network_type, ==, "Unknown");
  g_assert_cmpint (signal_strength, ==, -1);
  g_assert_cmpstr (icon_name, ==, "network-cellular-symbolic");

  g_clear_pointer (&signal_info, g_variant_unref);
  g_clear_pointer (&signal_strengths, g_variant_unref);
  g_clear_pointer (&state, g_variant_unref);

  VALENT_TEST_CHECK ("Plugin handles the \"none\" state");
  packet = valent_test_fixture_lookup_packet (fixture, "none-report");
  valent_test_fixture_handle_packet (fixture, packet);

  g_assert_true (g_action_group_get_action_enabled (actions, "connectivity_report.state"));
  state = g_action_group_get_action_state (actions, "connectivity_report.state");

  g_variant_lookup (state, "signal-strengths", "@a{sv}", &signal_strengths);
  g_variant_lookup (signal_strengths, "1", "@a{sv}", &signal_info);

  g_assert_true (g_variant_lookup (signal_info, "network-type", "&s", &network_type));
  g_assert_true (g_variant_lookup (signal_info, "signal-strength", "x", &signal_strength));
  g_assert_true (g_variant_lookup (signal_info, "icon-name", "&s", &icon_name));

  g_assert_cmpstr (network_type, ==, "GSM");
  g_assert_cmpint (signal_strength, ==, 0);
  g_assert_cmpstr (icon_name, ==, "network-cellular-2g-symbolic");

  g_clear_pointer (&signal_info, g_variant_unref);
  g_clear_pointer (&signal_strengths, g_variant_unref);
  g_clear_pointer (&state, g_variant_unref);

  VALENT_TEST_CHECK ("Plugin handles the \"weak\" state");
  packet = valent_test_fixture_lookup_packet (fixture, "weak-report");
  valent_test_fixture_handle_packet (fixture, packet);

  g_assert_true (g_action_group_get_action_enabled (actions, "connectivity_report.state"));
  state = g_action_group_get_action_state (actions, "connectivity_report.state");

  g_variant_lookup (state, "signal-strengths", "@a{sv}", &signal_strengths);
  g_variant_lookup (signal_strengths, "1", "@a{sv}", &signal_info);

  g_assert_true (g_variant_lookup (signal_info, "network-type", "&s", &network_type));
  g_assert_true (g_variant_lookup (signal_info, "signal-strength", "x", &signal_strength));
  g_assert_true (g_variant_lookup (signal_info, "icon-name", "&s", &icon_name));

  g_assert_cmpstr (network_type, ==, "UMTS");
  g_assert_cmpint (signal_strength, ==, 1);
  g_assert_cmpstr (icon_name, ==, "network-cellular-3g-symbolic");

  g_clear_pointer (&signal_info, g_variant_unref);
  g_clear_pointer (&signal_strengths, g_variant_unref);
  g_clear_pointer (&state, g_variant_unref);

  VALENT_TEST_CHECK ("Plugin handles the \"ok\" state");
  packet = valent_test_fixture_lookup_packet (fixture, "ok-report");
  valent_test_fixture_handle_packet (fixture, packet);

  g_assert_true (g_action_group_get_action_enabled (actions, "connectivity_report.state"));
  state = g_action_group_get_action_state (actions, "connectivity_report.state");

  g_variant_lookup (state, "signal-strengths", "@a{sv}", &signal_strengths);
  g_variant_lookup (signal_strengths, "1", "@a{sv}", &signal_info);

  g_assert_true (g_variant_lookup (signal_info, "icon-name", "&s", &icon_name));
  g_assert_true (g_variant_lookup (signal_info, "network-type", "&s", &network_type));
  g_assert_true (g_variant_lookup (signal_info, "signal-strength", "x", &signal_strength));

  g_assert_cmpstr (network_type, ==, "LTE");
  g_assert_cmpint (signal_strength, ==, 2);
  g_assert_cmpstr (icon_name, ==, "network-cellular-4g-symbolic");

  g_clear_pointer (&signal_info, g_variant_unref);
  g_clear_pointer (&signal_strengths, g_variant_unref);
  g_clear_pointer (&state, g_variant_unref);

  VALENT_TEST_CHECK ("Plugin handles the \"good\" state");
  packet = valent_test_fixture_lookup_packet (fixture, "good-report");
  valent_test_fixture_handle_packet (fixture, packet);

  state = g_action_group_get_action_state (actions, "connectivity_report.state");

  g_variant_lookup (state, "signal-strengths", "@a{sv}", &signal_strengths);
  g_variant_lookup (signal_strengths, "1", "@a{sv}", &signal_info);

  g_assert_true (g_variant_lookup (signal_info, "network-type", "&s", &network_type));
  g_assert_true (g_variant_lookup (signal_info, "signal-strength", "x", &signal_strength));
  g_assert_true (g_variant_lookup (signal_info, "icon-name", "&s", &icon_name));

  g_assert_cmpstr (network_type, ==, "EDGE");
  g_assert_cmpint (signal_strength, ==, 3);
  g_assert_cmpstr (icon_name, ==, "network-cellular-edge-symbolic");

  g_clear_pointer (&signal_info, g_variant_unref);
  g_clear_pointer (&signal_strengths, g_variant_unref);
  g_clear_pointer (&state, g_variant_unref);

  VALENT_TEST_CHECK ("Plugin handles the \"excellent\" state");
  packet = valent_test_fixture_lookup_packet (fixture, "excellent-report");
  valent_test_fixture_handle_packet (fixture, packet);

  g_assert_true (g_action_group_get_action_enabled (actions, "connectivity_report.state"));
  state = g_action_group_get_action_state (actions, "connectivity_report.state");

  g_variant_lookup (state, "signal-strengths", "@a{sv}", &signal_strengths);
  g_variant_lookup (signal_strengths, "1", "@a{sv}", &signal_info);

  g_assert_true (g_variant_lookup (signal_info, "network-type", "&s", &network_type));
  g_assert_true (g_variant_lookup (signal_info, "signal-strength", "x", &signal_strength));
  g_assert_true (g_variant_lookup (signal_info, "icon-name", "&s", &icon_name));

  g_assert_cmpstr (network_type, ==, "GPRS");
  g_assert_cmpint (signal_strength, ==, 4);
  g_assert_cmpstr (icon_name, ==, "network-cellular-gprs-symbolic");

  g_clear_pointer (&signal_info, g_variant_unref);
  g_clear_pointer (&signal_strengths, g_variant_unref);
  g_clear_pointer (&state, g_variant_unref);

  VALENT_TEST_CHECK ("Plugin handles other states");
  packet = valent_test_fixture_lookup_packet (fixture, "extra1-report");
  valent_test_fixture_handle_packet (fixture, packet);

  g_assert_true (g_action_group_get_action_enabled (actions, "connectivity_report.state"));
  state = g_action_group_get_action_state (actions, "connectivity_report.state");

  g_variant_lookup (state, "signal-strengths", "@a{sv}", &signal_strengths);
  g_variant_lookup (signal_strengths, "1", "@a{sv}", &signal_info);

  g_assert_true (g_variant_lookup (signal_info, "network-type", "&s", &network_type));
  g_assert_true (g_variant_lookup (signal_info, "signal-strength", "x", &signal_strength));
  g_assert_true (g_variant_lookup (signal_info, "icon-name", "&s", &icon_name));

  g_assert_cmpstr (network_type, ==, "HSPA");
  g_assert_cmpint (signal_strength, ==, 5);
  g_assert_cmpstr (icon_name, ==, "network-cellular-hspa-symbolic");

  g_clear_pointer (&signal_info, g_variant_unref);
  g_clear_pointer (&signal_strengths, g_variant_unref);
  g_clear_pointer (&state, g_variant_unref);

  VALENT_TEST_CHECK ("Plugin handles other states");
  packet = valent_test_fixture_lookup_packet (fixture, "extra2-report");
  valent_test_fixture_handle_packet (fixture, packet);

  g_assert_true (g_action_group_get_action_enabled (actions, "connectivity_report.state"));
  state = g_action_group_get_action_state (actions, "connectivity_report.state");

  g_variant_lookup (state, "signal-strengths", "@a{sv}", &signal_strengths);
  g_variant_lookup (signal_strengths, "1", "@a{sv}", &signal_info);

  g_assert_true (g_variant_lookup (signal_info, "network-type", "&s", &network_type));
  g_assert_true (g_variant_lookup (signal_info, "signal-strength", "x", &signal_strength));
  g_assert_true (g_variant_lookup (signal_info, "icon-name", "&s", &icon_name));

  g_assert_cmpstr (network_type, ==, "5G");
  g_assert_cmpint (signal_strength, ==, 5);
  g_assert_cmpstr (icon_name, ==, "network-cellular-5g-symbolic");

  g_clear_pointer (&signal_info, g_variant_unref);
  g_clear_pointer (&signal_strengths, g_variant_unref);
  g_clear_pointer (&state, g_variant_unref);
}

static void
test_connectivity_report_plugin_handle_request (ValentTestFixture *fixture,
                                                gconstpointer      user_data)
{
  g_autoptr (GDBusConnection) connection = NULL;
  JsonNode *packet;
  JsonObject *signal_node;
  JsonObject *signal_meta;
  const char *network_type;
  int64_t signal_strength;

  connection = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, NULL);

  VALENT_TEST_CHECK ("Plugin requests the connectivity status on connect");
  valent_test_fixture_connect (fixture, TRUE);

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.connectivity_report.request");
  json_node_unref (packet);

  VALENT_TEST_CHECK ("Plugin sends a connectivity update when requested");
  packet = valent_test_fixture_lookup_packet (fixture, "request-state");
  valent_test_fixture_handle_packet (fixture, packet);

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.connectivity_report");
  v_assert_packet_field (packet, "signalStrengths");
  valent_packet_get_object (packet, "signalStrengths", &signal_node);
  g_assert_cmpuint (json_object_get_size (signal_node), ==, 0);

  json_node_unref (packet);

  VALENT_TEST_CHECK ("Plugin sends an update when a modem is added");
  dbusmock_modemmanager (connection, "AddModem", 0);

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.connectivity_report");
  v_assert_packet_field (packet, "signalStrengths");
  valent_packet_get_object (packet, "signalStrengths", &signal_node);
  signal_meta = json_object_get_object_member (signal_node, "0");

  network_type = json_object_get_string_member (signal_meta, "networkType");
  signal_strength = json_object_get_int_member (signal_meta, "signalStrength");

  g_assert_cmpstr (network_type, ==, "Unknown");
  g_assert_cmpint (signal_strength, ==, -1);

  json_node_unref (packet);

  VALENT_TEST_CHECK ("Plugin sends an update when a modem comes online");
  dbusmock_modemmanager (connection, "SetModemOnline", 0);

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.connectivity_report");
  v_assert_packet_field (packet, "signalStrengths");
  valent_packet_get_object (packet, "signalStrengths", &signal_node);
  signal_meta = json_object_get_object_member (signal_node, "0");

  network_type = json_object_get_string_member (signal_meta, "networkType");
  signal_strength = json_object_get_int_member (signal_meta, "signalStrength");

  g_assert_cmpstr (network_type, ==, "LTE");
  g_assert_cmpint (signal_strength, ==, 3);

  json_node_unref (packet);

  VALENT_TEST_CHECK ("Plugin sends an update when a modem goes offline");
  dbusmock_modemmanager (connection, "SetModemOffline", 0);

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.connectivity_report");
  v_assert_packet_field (packet, "signalStrengths");
  valent_packet_get_object (packet, "signalStrengths", &signal_node);
  signal_meta = json_object_get_object_member (signal_node, "0");

  network_type = json_object_get_string_member (signal_meta, "networkType");
  signal_strength = json_object_get_int_member (signal_meta, "signalStrength");

  g_assert_cmpstr (network_type, ==, "Unknown");
  g_assert_cmpint (signal_strength, ==, -1);

  json_node_unref (packet);

  VALENT_TEST_CHECK ("Plugin sends an update when a modem is removed");
  dbusmock_modemmanager (connection, "RemoveModem", 0);

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.connectivity_report");
  v_assert_packet_field (packet, "signalStrengths");
  valent_packet_get_object (packet, "signalStrengths", &signal_node);
  g_assert_cmpuint (json_object_get_size (signal_node), ==, 0);

  json_node_unref (packet);
}

static const char *schemas[] = {
  /* "/tests/kdeconnect.connectivity_report.json", */
  "/tests/kdeconnect.connectivity_report.request.json",
};

static void
test_connectivity_report_plugin_fuzz (ValentTestFixture *fixture,
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
  const char *path = "plugin-connectivity_report.json";

  valent_test_init (&argc, &argv, NULL);

  g_test_add ("/plugins/connectivity_report/actions",
              ValentTestFixture, path,
              valent_test_fixture_init,
              test_connectivity_report_plugin_actions,
              valent_test_fixture_clear);

  g_test_add ("/plugins/connectivity_report/connect",
              ValentTestFixture, path,
              valent_test_fixture_init,
              test_connectivity_report_plugin_connect,
              valent_test_fixture_clear);

  g_test_add ("/plugins/connectivity_report/handle-update",
              ValentTestFixture, path,
              valent_test_fixture_init,
              test_connectivity_report_plugin_handle_update,
              valent_test_fixture_clear);

  g_test_add ("/plugins/connectivity_report/handle-request",
              ValentTestFixture, path,
              valent_test_fixture_init,
              test_connectivity_report_plugin_handle_request,
              valent_test_fixture_clear);

  g_test_add ("/plugins/connectivity_report/fuzz",
              ValentTestFixture, path,
              valent_test_fixture_init,
              test_connectivity_report_plugin_fuzz,
              valent_test_fixture_clear);

  return g_test_run ();
}
