#include <gio/gio.h>
#include <libvalent-core.h>
#include <libvalent-power.h>
#include <libvalent-test.h>


typedef struct
{
  ValentPower       *power;
  ValentPowerDevice *device;
  gpointer           data;
} PowerComponentFixture;

static void
on_device_added (GObject               *object,
                 ValentPowerDevice     *device,
                 PowerComponentFixture *fixture)
{
  fixture->data = object;
}

static void
on_device_removed (GObject               *object,
                   ValentPowerDevice     *device,
                   PowerComponentFixture *fixture)
{
  fixture->data = object;
}

static void
power_component_fixture_set_up (PowerComponentFixture *fixture,
                                gconstpointer          user_data)
{
  fixture->power = valent_power_get_default ();

  fixture->device = g_object_new (VALENT_TYPE_MOCK_POWER_DEVICE, NULL);
  valent_mock_power_device_set_kind (VALENT_MOCK_POWER_DEVICE (fixture->device),
                                     VALENT_POWER_DEVICE_BATTERY);
}

static void
power_component_fixture_tear_down (PowerComponentFixture *fixture,
                                   gconstpointer          user_data)
{
  g_assert_finalize_object (fixture->power);
  g_assert_finalize_object (fixture->device);
}

static void
test_power_component_provider (PowerComponentFixture *fixture,
                               gconstpointer          user_data)
{
  g_autoptr (GPtrArray) providers = NULL;
  ValentPowerDeviceProvider *provider;

  /* Wait for valent_power_device_provider_load_async() to resolve */
  providers = valent_component_get_providers (VALENT_COMPONENT (fixture->power));
  provider = g_ptr_array_index (providers, 0);

  while (g_main_context_iteration (NULL, FALSE))
    continue;

  g_signal_connect (provider,
                    "device-added",
                    G_CALLBACK (on_device_added),
                    fixture);

  valent_power_device_provider_emit_device_added (provider, fixture->device);
  g_assert_true (fixture->data == provider);
  fixture->data = NULL;

  g_signal_connect (provider,
                    "device-removed",
                    G_CALLBACK (on_device_removed),
                    fixture);

  valent_power_device_provider_emit_device_removed (provider, fixture->device);
  g_assert_true (fixture->data == provider);
  fixture->data = NULL;
}

static void
test_power_component_device (PowerComponentFixture *fixture,
                             gconstpointer          user_data)
{
  g_autoptr (GPtrArray) providers = NULL;
  ValentPowerDeviceProvider *provider;
  PeasPluginInfo *info;
  ValentPowerKind kind;
  int level;
  ValentPowerState state;
  ValentPowerWarning warning;

  /* Wait for valent_power_device_provider_load_async() to resolve */
  providers = valent_component_get_providers (VALENT_COMPONENT (fixture->power));
  provider = g_ptr_array_index (providers, 0);

  while (g_main_context_iteration (NULL, FALSE))
    continue;

  /* Properties */
  g_object_get (provider,
                "plugin-info", &info,
                NULL);
  g_assert_nonnull (info);
  g_boxed_free (PEAS_TYPE_PLUGIN_INFO, info);

  /* Add Device */
  g_signal_connect (provider,
                    "device-added",
                    G_CALLBACK (on_device_added),
                    fixture);

  valent_power_device_provider_emit_device_added (provider, fixture->device);
  g_assert_true (fixture->data == provider);
  fixture->data = NULL;

  /* Test Device */
  g_object_get (fixture->device,
                "kind",    &kind,
                "level",   &level,
                "state",   &state,
                "warning", &warning,
                NULL);

  g_assert_cmpuint (kind, ==, VALENT_POWER_DEVICE_BATTERY);
  g_assert_cmpint (level, ==, -1);
  g_assert_cmpuint (state, ==, VALENT_POWER_STATE_UNKNOWN);
  g_assert_cmpuint (warning, ==, VALENT_POWER_WARNING_NONE);

  /* Remove Device */
  g_signal_connect (provider,
                    "device-removed",
                    G_CALLBACK (on_device_removed),
                    fixture);

  valent_power_device_provider_emit_device_removed (provider, fixture->device);
  g_assert_true (fixture->data == provider);
  fixture->data = NULL;
}

static void
test_power_component_self (PowerComponentFixture *fixture,
                           gconstpointer          user_data)
{
  g_autoptr (GPtrArray) providers = NULL;
  ValentPowerDeviceProvider *provider;
  int level1, level2;
  ValentPowerState state1, state2;
  ValentPowerWarning warning1, warning2;

  /* Wait for valent_power_device_provider_load_async() to resolve */
  providers = valent_component_get_providers (VALENT_COMPONENT (fixture->power));
  provider = g_ptr_array_index (providers, 0);

  while (g_main_context_iteration (NULL, FALSE))
    continue;

  /* Add Device */
  g_signal_connect (provider,
                    "device-added",
                    G_CALLBACK (on_device_added),
                    fixture);

  valent_power_device_provider_emit_device_added (provider, fixture->device);
  g_assert_true (fixture->data == provider);
  fixture->data = NULL;

  /* Change Device */
  valent_mock_power_device_set_level (VALENT_MOCK_POWER_DEVICE (fixture->device),
                                      42);

  /* Compare Device & Aggregator */
  g_object_get (fixture->device,
                "level",   &level1,
                "state",   &state1,
                "warning", &warning1,
                NULL);

  g_object_get (fixture->power,
                "battery-level",   &level2,
                "battery-state",   &state2,
                "battery-warning", &warning2,
                NULL);

  g_assert_cmpint (level1, ==, level2);
  g_assert_cmpuint (state1, ==, state2);
  g_assert_cmpuint (warning1, ==, warning2);

  /* Remove Device */
  g_signal_connect (provider,
                    "device-removed",
                    G_CALLBACK (on_device_removed),
                    fixture);

  valent_power_device_provider_emit_device_removed (provider, fixture->device);
  g_assert_true (fixture->data == provider);
  fixture->data = NULL;
}

static void
test_power_component_dispose (PowerComponentFixture *fixture,
                              gconstpointer          user_data)
{
  GPtrArray *providers;
  ValentPowerDeviceProvider *provider;
  PeasEngine *engine;

  /* Add a device to the provider */
  providers = valent_component_get_providers (VALENT_COMPONENT (fixture->power));
  provider = g_ptr_array_index (providers, 0);
  g_ptr_array_unref (providers);

  /* Wait for provider to resolve */
  valent_power_device_provider_emit_device_added (provider, fixture->device);

  while (g_main_context_iteration (NULL, FALSE))
    continue;

  /* Unload the provider */
  engine = valent_get_engine ();
  peas_engine_unload_plugin (engine, peas_engine_get_plugin_info (engine, "mock"));

  providers = valent_component_get_providers (VALENT_COMPONENT (fixture->power));
  g_assert_cmpuint (providers->len, ==, 0);
  g_ptr_array_unref (providers);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, G_TEST_OPTION_ISOLATE_DIRS, NULL);

  g_test_add ("/components/power/provider",
              PowerComponentFixture, NULL,
              power_component_fixture_set_up,
              test_power_component_provider,
              power_component_fixture_tear_down);

  g_test_add ("/components/power/device",
              PowerComponentFixture, NULL,
              power_component_fixture_set_up,
              test_power_component_device,
              power_component_fixture_tear_down);

  g_test_add ("/components/power/self",
              PowerComponentFixture, NULL,
              power_component_fixture_set_up,
              test_power_component_self,
              power_component_fixture_tear_down);

  g_test_add ("/components/power/dispose",
              PowerComponentFixture, NULL,
              power_component_fixture_set_up,
              test_power_component_dispose,
              power_component_fixture_tear_down);

  return g_test_run ();
}
