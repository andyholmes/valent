#include <glib.h>
#include <libvalent-core.h>
#include <libvalent-power.h>

#include "valent-upower-device-provider.h"


typedef struct
{
  GDBusConnection   *connection;
  GMainLoop         *loop;
  ValentPowerDevice *device;
} UPowerFixture;


static void
upower_fixture_setup (UPowerFixture *fixture,
                      gconstpointer  user_data)
{
  fixture->connection = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, NULL);
  fixture->loop = g_main_loop_new (NULL, FALSE);
}

static void
upower_fixture_teardown (UPowerFixture *fixture,
                         gconstpointer  user_data)
{
  g_clear_object (&fixture->connection);
  g_clear_pointer (&fixture->loop, g_main_loop_unref);
}

static void
dbusmock_cb (GDBusConnection *connection,
             GAsyncResult    *result,
             UPowerFixture   *fixture)
{
  g_autoptr (GVariant) reply = NULL;
  GError *error = NULL;

  reply = g_dbus_connection_call_finish (connection, result, &error);
  g_assert_no_error (error);
}

static void
load_cb (ValentPowerDeviceProvider *provider,
         GAsyncResult              *result,
         UPowerFixture             *fixture)
{
  GError *error = NULL;

  valent_power_device_provider_load_finish (provider, result, &error);
  g_assert_no_error (error);

  g_main_loop_quit (fixture->loop);
}

static void
on_device_added (ValentPowerDeviceProvider *provider,
                 ValentPowerDevice         *device,
                 UPowerFixture             *fixture)
{
  fixture->device = device;
  g_main_loop_quit (fixture->loop);
}

static void
on_device_removed (ValentPowerDeviceProvider *provider,
                   ValentPowerDevice         *device,
                   UPowerFixture             *fixture)
{
  fixture->device = NULL;
  g_main_loop_quit (fixture->loop);
}

static void
upower_mock_call (UPowerFixture *fixture,
                  const char    *method,
                  GVariant      *args)
{
  g_dbus_connection_call (fixture->connection,
                          "org.freedesktop.UPower",
                          "/org/freedesktop/UPower",
                          "org.freedesktop.DBus.Mock",
                          method,
                          args,
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          NULL,
                          (GAsyncReadyCallback)dbusmock_cb,
                          fixture);
}

static void
test_upower_basic (UPowerFixture *fixture,
                   gconstpointer  user_data)
{
  PeasEngine *engine;
  PeasPluginInfo *info;
  ValentPowerDeviceProvider *provider;
  const char *object_path;
  GVariant *parameters;

  engine = valent_get_engine ();
  info = peas_engine_get_plugin_info (engine, "upower");
  provider = g_object_new (VALENT_TYPE_UPOWER_DEVICE_PROVIDER,
                            "plugin-info", info,
                            NULL);

  g_signal_connect (provider,
                    "device-added",
                    G_CALLBACK (on_device_added),
                    fixture);
  g_signal_connect (provider,
                    "device-removed",
                    G_CALLBACK (on_device_removed),
                    fixture);

  g_assert_nonnull (provider);
  g_assert_true (VALENT_IS_UPOWER_DEVICE_PROVIDER (provider));

  /* Load provider */
  valent_power_device_provider_load_async (VALENT_POWER_DEVICE_PROVIDER (provider),
                                           NULL,
                                           (GAsyncReadyCallback)load_cb,
                                           fixture);
  g_main_loop_run (fixture->loop);

  /* Add Device */
  object_path = "/org/freedesktop/UPower/devices/mock_BAT";

  parameters = g_variant_new ("(ssdx)", "mock_BAT", "Mock Battery", 30.0, 1200);
  upower_mock_call (fixture, "AddChargingBattery", parameters);
  g_main_loop_run (fixture->loop);

  /* Check Properties */
  ValentPowerKind kind;
  ValentPowerState state;
  int level;
  ValentPowerWarning warning;

  g_object_get (fixture->device,
                "kind",    &kind,
                "level",   &level,
                "state",   &state,
                "warning", &warning,
                NULL);

  g_assert_cmpint (level, ==, 30);
  g_assert_cmpuint (kind, ==, VALENT_POWER_DEVICE_BATTERY);
  g_assert_cmpuint (state, ==, VALENT_POWER_STATE_CHARGING);
  g_assert_cmpuint (warning, ==, VALENT_POWER_WARNING_NONE);

  /* Change Properties */
  GVariantDict dict;

  g_variant_dict_init (&dict, NULL);
  g_variant_dict_insert (&dict, "Percentage", "d", 15.0);
  g_variant_dict_insert (&dict, "State", "u", 2);
  parameters = g_variant_new ("(o@a{sv})", object_path, g_variant_dict_end (&dict));

  upower_mock_call (fixture, "SetDeviceProperties", parameters);

  /* Remove Device */
  parameters = g_variant_new ("(s)", object_path);
  upower_mock_call (fixture, "RemoveObject", parameters);

  GVariant *args[] = {
    g_variant_new_variant (g_variant_new_object_path (object_path)),
  };

  parameters = g_variant_new ("(sss@av)",
                              "org.freedesktop.UPower",
                              "DeviceRemoved",
                              "o",
                              g_variant_new_array (NULL, args, 1));
  upower_mock_call (fixture, "EmitSignal", parameters);
  g_main_loop_run (fixture->loop);

  g_object_unref (provider);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, G_TEST_OPTION_ISOLATE_DIRS, NULL);

  g_test_add ("/plugins/upower/basic",
              UPowerFixture, NULL,
              upower_fixture_setup,
              test_upower_basic,
              upower_fixture_teardown);

  return g_test_run ();
}
