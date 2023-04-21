// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>
#include <valent.h>
#include <libvalent-test.h>

#include "valent-battery.h"

#define DEVICE_PATH "/org/freedesktop/UPower/devices/DisplayDevice"

enum {
  UPOWER_LEVEL_UNKNOWN,
  UPOWER_LEVEL_NONE,
  UPOWER_LEVEL_DISCHARGING,
  UPOWER_LEVEL_LOW,
  UPOWER_LEVEL_CRITICAL,
  UPOWER_LEVEL_ACTION,
  UPOWER_LEVEL_NORMAL,
  UPOWER_LEVEL_HIGH,
  UPOWER_LEVEL_FULL
};

enum {
  UPOWER_STATE_UNKNOWN,
  UPOWER_STATE_CHARGING,
  UPOWER_STATE_DISCHARGING,
  UPOWER_STATE_EMPTY,
  UPOWER_STATE_FULLY_CHARGED,
  UPOWER_STATE_PENDING_CHARGE,
  UPOWER_STATE_PENDING_DISCHARGE
};


static void
set_device_properties (GDBusConnection *connection,
                       gboolean         is_present,
                       double           percentage,
                       uint32_t          state,
                       uint32_t          warning_level)
{
  GVariantDict dict;

  g_variant_dict_init (&dict, NULL);
  g_variant_dict_insert (&dict, "IsPresent", "b", is_present);
  g_variant_dict_insert (&dict, "Percentage", "d", percentage);
  g_variant_dict_insert (&dict, "State", "u", state);
  g_variant_dict_insert (&dict, "WarningLevel", "u", warning_level);

  g_dbus_connection_call (connection,
                          "org.freedesktop.UPower",
                          "/org/freedesktop/UPower",
                          "org.freedesktop.DBus.Mock",
                          "SetDeviceProperties",
                          g_variant_new ("(o@a{sv})",
                                         DEVICE_PATH,
                                         g_variant_dict_end (&dict)),
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          NULL,
                          NULL,
                          NULL);
}

static void
test_battery_proxy (void)
{
  g_autoptr (GMainLoop) loop = NULL;
  g_autoptr (ValentBattery) battery = NULL;
  g_autoptr (GDBusConnection) connection = NULL;

  /* Setup the battery */
  loop = g_main_loop_new (NULL, FALSE);
  battery = valent_battery_get_default ();
  connection = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, NULL);

  g_signal_connect_swapped (battery,
                            "changed",
                            G_CALLBACK (g_main_loop_quit),
                            loop);

  /* Initial State */
  g_assert_cmpint (valent_battery_current_charge (battery), ==, 0);
  g_assert_false (valent_battery_is_charging (battery));
  g_assert_false (valent_battery_is_present (battery));
  g_assert_cmpuint (valent_battery_threshold_event (battery), ==, 0);

  // NOTE: ValentBattery emits ::changed once when it initializes properties
  g_main_loop_run (loop);

  /* Initial Properties */
  g_assert_cmpint (valent_battery_current_charge (battery), ==, 0);
  g_assert_false (valent_battery_is_charging (battery));
  g_assert_true (valent_battery_is_present (battery));
  g_assert_cmpuint (valent_battery_threshold_event (battery), ==, 0);

  /* Percentage */
  set_device_properties (connection, TRUE, 42.0, 0, 0);
  g_main_loop_run (loop);
  g_assert_cmpint (valent_battery_current_charge (battery), ==, 42);

  set_device_properties (connection, TRUE, 0.0, 0, 0);
  g_main_loop_run (loop);
  g_assert_cmpint (valent_battery_current_charge (battery), ==, 0);


  /* Check device states correspend to correct `isCharging` values */
  set_device_properties (connection, TRUE, 0.0, UPOWER_STATE_CHARGING, 0);
  g_main_loop_run (loop);
  g_assert_true (valent_battery_is_charging (battery));

  set_device_properties (connection, TRUE, 0.0, UPOWER_STATE_DISCHARGING, 0);
  g_main_loop_run (loop);
  g_assert_false (valent_battery_is_charging (battery));

  set_device_properties (connection, TRUE, 0.0, UPOWER_STATE_PENDING_CHARGE, 0);
  g_main_loop_run (loop);
  g_assert_true (valent_battery_is_charging (battery));

  set_device_properties (connection, TRUE, 0.0, UPOWER_STATE_PENDING_DISCHARGE, 0);
  g_main_loop_run (loop);
  g_assert_false (valent_battery_is_charging (battery));

  set_device_properties (connection, TRUE, 0.0, UPOWER_STATE_FULLY_CHARGED, 0);
  g_main_loop_run (loop);
  g_assert_true (valent_battery_is_charging (battery));

  set_device_properties (connection, TRUE, 0.0, UPOWER_STATE_EMPTY, 0);
  g_main_loop_run (loop);
  g_assert_false (valent_battery_is_charging (battery));


  /* Check warning levels correspond to correct `thresholdEvent` values */
  set_device_properties (connection, TRUE, 0.0, 0, UPOWER_LEVEL_LOW);
  g_main_loop_run (loop);
  g_assert_cmpuint (valent_battery_threshold_event (battery), ==, 1);

  set_device_properties (connection, TRUE, 0.0, 0, UPOWER_LEVEL_NONE);
  g_main_loop_run (loop);
  g_assert_cmpuint (valent_battery_threshold_event (battery), ==, 0);

  set_device_properties (connection, TRUE, 0.0, 0, UPOWER_LEVEL_CRITICAL);
  g_main_loop_run (loop);
  g_assert_cmpuint (valent_battery_threshold_event (battery), ==, 1);

  set_device_properties (connection, TRUE, 0.0, 0, UPOWER_LEVEL_NONE);
  g_main_loop_run (loop);
  g_assert_cmpuint (valent_battery_threshold_event (battery), ==, 0);

  set_device_properties (connection, TRUE, 0.0, 0, UPOWER_LEVEL_ACTION);
  g_main_loop_run (loop);
  g_assert_cmpuint (valent_battery_threshold_event (battery), ==, 1);


  /* Check battery insertion/removal is handled */
  set_device_properties (connection, FALSE, 0.0, 0, 0);
  g_main_loop_run (loop);

  g_assert_cmpint (valent_battery_current_charge (battery), ==, 0);
  g_assert_false (valent_battery_is_charging (battery));
  g_assert_false (valent_battery_is_present (battery));
  g_assert_cmpuint (valent_battery_threshold_event (battery), ==, 0);

  set_device_properties (connection, TRUE, 0.0, 0, 0);
  g_main_loop_run (loop);

  g_assert_cmpint (valent_battery_current_charge (battery), ==, 0);
  g_assert_false (valent_battery_is_charging (battery));
  g_assert_true (valent_battery_is_present (battery));
  g_assert_cmpuint (valent_battery_threshold_event (battery), ==, 0);


  g_signal_handlers_disconnect_by_data (battery, loop);
}

int
main (int   argc,
      char *argv[])
{
  valent_test_init (&argc, &argv, NULL);

  g_test_add_func ("/plugins/battery/proxy",
                   test_battery_proxy);

  return g_test_run ();
}

