#include <gio/gio.h>
#include <libvalent-core.h>
#include <libvalent-test.h>

#include "valent-battery.h"

#define DEVICE_PATH "/org/freedesktop/UPower/devices/DisplayDevice"


static void
test_battery_proxy (void)
{
  g_autoptr (ValentBattery) battery = NULL;
  g_autoptr (GDBusConnection) connection = NULL;
  g_autoptr (GMainLoop) loop = NULL;
  GVariantDict dict;
  GVariant *args;
  gboolean charging;
  int level;
  unsigned int threshold;

  battery = valent_battery_get_default ();
  connection = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, NULL);
  loop = g_main_loop_new (NULL, FALSE);

  g_signal_connect_swapped (battery,
                            "changed",
                            G_CALLBACK (g_main_loop_quit),
                            loop);
  g_main_loop_run (loop);

  /* Initial Properties */
  charging = valent_battery_get_charging (battery);
  level = valent_battery_get_level (battery);
  threshold = valent_battery_get_threshold (battery);

  g_assert_false (charging);
  g_assert_cmpint (level, ==, 0);
  g_assert_cmpint (threshold, ==, 0);

  /* Change Properties */
  g_variant_dict_init (&dict, NULL);
  g_variant_dict_insert (&dict, "Percentage", "d", 42.0);
  g_variant_dict_insert (&dict, "State", "u", 1);
  g_variant_dict_insert (&dict, "WarningLevel", "u", 3);
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

  // NOTE: we expect three signal emissions
  g_main_loop_run (loop);
  g_main_loop_run (loop);
  g_main_loop_run (loop);

  g_object_get (battery,
                "charging",  &charging,
                "level",     &level,
                "threshold", &threshold,
                NULL);
  g_assert_true (charging);
  g_assert_cmpint (level, ==, 42);
  g_assert_cmpint (threshold, ==, 1);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, G_TEST_OPTION_ISOLATE_DIRS, NULL);

  g_test_add_func ("/plugins/battery/proxy",
                   test_battery_proxy);

  return g_test_run ();
}

