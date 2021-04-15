#include <libvalent-test.h>
#include <libvalent-ui.h>

#include "valent-device-panel.h"


static void
test_device_panel_basic (void)
{
  ValentTestPluginFixture *fixture;
  ValentDevicePanel *panel;
  ValentDevice *device = NULL;
  PeasEngine *engine;
  GtkWidget *widget = NULL;

  fixture = valent_test_plugin_fixture_new (TEST_DATA_DIR"/plugin-battery.json");

  widget = valent_device_panel_new (fixture->device);
  panel = g_object_ref_sink (VALENT_DEVICE_PANEL (widget));
  g_assert_nonnull (panel);

  /* Properties */
  g_object_get (panel,
                "device", &device,
                NULL);
  g_assert_true (fixture->device == device);
  g_object_unref (device);

  /* Unload the plugin */
  engine = valent_get_engine ();
  peas_engine_unload_plugin (engine,
                             peas_engine_get_plugin_info (engine, "battery"));

  g_clear_object (&panel);
  g_clear_pointer (&fixture, valent_test_plugin_fixture_free);
}

int
main (int argc,
     char *argv[])
{
  valent_test_ui_init (&argc, &argv, G_TEST_OPTION_ISOLATE_DIRS, NULL);

  g_test_add_func ("/libvalent/ui/device-panel",
                   test_device_panel_basic);

  return g_test_run ();
}

