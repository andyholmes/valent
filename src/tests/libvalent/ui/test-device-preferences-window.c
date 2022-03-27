// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <libvalent-test.h>
#include <libvalent-ui.h>

#include "valent-device-preferences-window.h"


static void
test_device_preferences_window_basic (void)
{
  ValentTestFixture *fixture;
  GtkWindow *window;
  ValentDevice *device = NULL;
  PeasEngine *engine;

  fixture = valent_test_fixture_new (TEST_DATA_DIR"/plugin-mock.json");

  window = g_object_new (VALENT_TYPE_DEVICE_PREFERENCES_WINDOW,
                        "device", fixture->device,
                        NULL);
  g_assert_true (VALENT_IS_DEVICE_PREFERENCES_WINDOW (window));

  /* Properties */
  g_object_get (window,
                "device", &device,
                NULL);
  g_assert_true (fixture->device == device);
  g_object_unref (device);

  /* Unload the plugin */
  engine = valent_get_engine ();
  peas_engine_unload_plugin (engine,
                             peas_engine_get_plugin_info (engine, "mock"));

  g_clear_pointer (&window, gtk_window_destroy);
  g_clear_pointer (&fixture, valent_test_fixture_unref);
}

int
main (int   argc,
      char *argv[])
{
  valent_test_ui_init (&argc, &argv, NULL);

  g_test_add_func ("/libvalent/ui/device-preferences-window",
                   test_device_preferences_window_basic);

  return g_test_run ();
}

