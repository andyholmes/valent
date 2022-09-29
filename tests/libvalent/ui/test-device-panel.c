// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <libvalent-test.h>
#include <libvalent-ui.h>

#include "valent-device-panel.h"


static void
test_device_panel_basic (ValentTestFixture *fixture,
                         gconstpointer      user_data)
{
  GtkWindow *window;
  GtkWidget *panel;
  ValentDevice *device = NULL;
  PeasEngine *engine;

  panel = g_object_new (VALENT_TYPE_DEVICE_PANEL,
                        "device", fixture->device,
                        NULL);
  g_assert_true (VALENT_IS_DEVICE_PANEL (panel));

  window = g_object_new (ADW_TYPE_WINDOW,
                         "content", panel,
                         NULL);
  gtk_window_present (window);

  while (g_main_context_iteration (NULL, FALSE))
    continue;

  /* Properties */
  g_object_get (panel,
                "device", &device,
                NULL);
  g_assert_true (fixture->device == device);
  g_clear_object (&device);

  /* Unload the plugin */
  engine = valent_get_plugin_engine ();
  peas_engine_unload_plugin (engine,
                             peas_engine_get_plugin_info (engine, "mock"));

  g_clear_pointer (&window, gtk_window_destroy);
}

static void
test_device_panel_dialogs (ValentTestFixture *fixture,
                           gconstpointer      user_data)
{
  GtkWindow *window;
  GtkWidget *panel;

  panel = g_object_new (VALENT_TYPE_DEVICE_PANEL,
                        "device", fixture->device,
                        NULL);
  g_assert_true (VALENT_IS_DEVICE_PANEL (panel));

  window = g_object_new (ADW_TYPE_WINDOW,
                         "content", panel,
                         NULL);
  gtk_window_present (window);

  while (g_main_context_iteration (NULL, FALSE))
    continue;

  /* Preferences can be opened and closed */
  gtk_widget_activate_action (panel, "panel.preferences", NULL);

  while (g_main_context_iteration (NULL, FALSE))
    continue;

  valent_device_panel_close_preferences (VALENT_DEVICE_PANEL (panel));

  while (g_main_context_iteration (NULL, FALSE))
    continue;

  /* Closing the window closes the preferences */
  gtk_widget_activate_action (panel, "panel.preferences", NULL);

  while (g_main_context_iteration (NULL, FALSE))
    continue;

  g_clear_pointer (&window, gtk_window_destroy);
}

int
main (int   argc,
      char *argv[])
{
  const char *path = TEST_DATA_DIR"/plugin-mock.json";

  valent_test_ui_init (&argc, &argv, NULL);

  g_test_add ("/libvalent/ui/device-panel/basic",
              ValentTestFixture, path,
              valent_test_fixture_init,
              test_device_panel_basic,
              valent_test_fixture_clear);

  g_test_add ("/libvalent/ui/device-panel/dialogs",
              ValentTestFixture, path,
              valent_test_fixture_init,
              test_device_panel_dialogs,
              valent_test_fixture_clear);

  return g_test_run ();
}

