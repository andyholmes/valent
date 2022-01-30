// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <libvalent-test.h>
#include <libvalent-ui.h>

#include "valent-device-panel.h"


static void
test_device_panel_basic (void)
{
  ValentTestPluginFixture *fixture;
  GtkWidget *panel;
  ValentDevice *device = NULL;
  PeasEngine *engine;

  fixture = valent_test_plugin_fixture_new (TEST_DATA_DIR"/plugin-mock.json");

  panel = g_object_new (VALENT_TYPE_DEVICE_PANEL,
                        "device", fixture->device,
                        NULL);
  g_object_ref_sink (panel);
  g_assert_true (VALENT_IS_DEVICE_PANEL (panel));

  /* Properties */
  g_object_get (panel,
                "device", &device,
                NULL);
  g_assert_true (fixture->device == device);
  g_object_unref (device);

  /* Unload the plugin */
  engine = valent_get_engine ();
  peas_engine_unload_plugin (engine,
                             peas_engine_get_plugin_info (engine, "mock"));

  g_clear_object (&panel);
  g_clear_pointer (&fixture, valent_test_plugin_fixture_free);
}

int
main (int   argc,
      char *argv[])
{
  valent_test_ui_init (&argc, &argv, NULL);

  g_test_add_func ("/libvalent/ui/device-panel",
                   test_device_panel_basic);

  return g_test_run ();
}

