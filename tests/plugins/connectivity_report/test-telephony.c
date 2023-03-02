// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>
#include <valent.h>
#include <libvalent-test.h>

#include "valent-telephony.h"

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
test_telephony_proxy (void)
{
  g_autoptr (GMainLoop) loop = NULL;
  g_autoptr (ValentTelephony) telephony = NULL;
  g_autoptr (GDBusConnection) connection = NULL;
  g_autoptr (JsonNode) signal_node = NULL;
  JsonObject *signal_obj, *signal_meta;

  /* Setup the network */
  loop = g_main_loop_new (NULL, FALSE);
  telephony = valent_telephony_get_default ();
  connection = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, NULL);

  g_signal_connect_swapped (telephony,
                            "changed",
                            G_CALLBACK (g_main_loop_quit),
                            loop);

  /* Modem should be offline */
  dbusmock_modemmanager (connection, "AddModem", 0);
  g_main_loop_run (loop);

  signal_node = valent_telephony_get_signal_strengths (telephony);
  signal_obj = json_node_get_object (signal_node);
  signal_meta = json_object_get_object_member (signal_obj, "0");

  g_assert_cmpstr (json_object_get_string_member (signal_meta, "networkType"), ==, "Unknown");
  g_assert_cmpint (json_object_get_int_member (signal_meta, "signalStrength"), ==, -1);
  g_clear_pointer (&signal_node, json_node_unref);

  /* Modem should be online */
  dbusmock_modemmanager (connection, "SetModemOnline", 0);
  g_main_loop_run (loop);

  signal_node = valent_telephony_get_signal_strengths (telephony);
  signal_obj = json_node_get_object (signal_node);
  signal_meta = json_object_get_object_member (signal_obj, "0");

  g_assert_cmpstr (json_object_get_string_member (signal_meta, "networkType"), ==, "LTE");
  g_assert_cmpint (json_object_get_int_member (signal_meta, "signalStrength"), ==, 3);
  g_clear_pointer (&signal_node, json_node_unref);

  /* Modem should be offline */
  dbusmock_modemmanager (connection, "SetModemOffline", 0);
  g_main_loop_run (loop);

  signal_node = valent_telephony_get_signal_strengths (telephony);
  signal_obj = json_node_get_object (signal_node);
  signal_meta = json_object_get_object_member (signal_obj, "0");

  g_assert_cmpstr (json_object_get_string_member (signal_meta, "networkType"), ==, "Unknown");
  g_assert_cmpint (json_object_get_int_member (signal_meta, "signalStrength"), ==, -1);
  g_clear_pointer (&signal_node, json_node_unref);

  /* Modem should be removed */
  dbusmock_modemmanager (connection, "RemoveModem", 0);
  g_main_loop_run (loop);

  signal_node = valent_telephony_get_signal_strengths (telephony);
  signal_obj = json_node_get_object (signal_node);

  g_assert_cmpuint (json_object_get_size (signal_obj), ==, 0);

  g_signal_handlers_disconnect_by_data (telephony, loop);
}

int
main (int   argc,
      char *argv[])
{
  valent_test_init (&argc, &argv, NULL);

  g_test_add_func ("/plugins/connectivity_report/telephony",
                   test_telephony_proxy);

  return g_test_run ();
}

