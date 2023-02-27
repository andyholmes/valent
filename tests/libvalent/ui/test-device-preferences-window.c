// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <valent.h>
#include <libvalent-test.h>

#include "valent-device-preferences-window.h"


static void
test_device_preference_window_basic (ValentTestFixture *fixture,
                                     gconstpointer      user_data)
{
  GtkWindow *window;
  ValentDevice *device;
  PeasEngine *engine;
  PeasPluginInfo *info;

  window = g_object_new (VALENT_TYPE_DEVICE_PREFERENCES_WINDOW,
                         "device", fixture->device,
                         NULL);
  g_assert_true (VALENT_IS_DEVICE_PREFERENCES_WINDOW (window));

  /* Show the window */
  gtk_window_present (window);

  while (g_main_context_iteration (NULL, FALSE))
    continue;

  /* Properties */
  g_object_get (window,
                "device", &device,
                NULL);
  g_assert_true (fixture->device == device);
  g_clear_object (&device);

  /* Unload/Load the plugin */
  engine = valent_get_plugin_engine ();
  info = peas_engine_get_plugin_info (engine, "mock");
  peas_engine_unload_plugin (engine, info);
  peas_engine_load_plugin (engine, info);

  g_clear_pointer (&window, gtk_window_destroy);
}

static void
test_device_preference_window_navigation (ValentTestFixture *fixture,
                                          gconstpointer      user_data)
{
  GtkWindow *window;

  window = g_object_new (VALENT_TYPE_DEVICE_PREFERENCES_WINDOW,
                         "device", fixture->device,
                         NULL);
  g_assert_true (VALENT_IS_DEVICE_PREFERENCES_WINDOW (window));

  g_object_add_weak_pointer (G_OBJECT (window), (gpointer)&window);
  gtk_window_present (window);

  while (g_main_context_iteration (NULL, FALSE))
    continue;

  /* Main -> Plugin */
  gtk_widget_activate_action (GTK_WIDGET (window), "win.page", "s", "mock");

  while (g_main_context_iteration (NULL, FALSE))
    continue;

  /* Plugin -> Main */
  gtk_widget_activate_action (GTK_WIDGET (window), "win.previous", NULL);

  while (g_main_context_iteration (NULL, FALSE))
    continue;

  /* Main -> Close Window */
  gtk_widget_activate_action (GTK_WIDGET (window), "win.previous", NULL);

  while (g_main_context_iteration (NULL, FALSE))
    continue;

  g_assert_null (window);
}

int
main (int   argc,
      char *argv[])
{
  const char *path = "plugin-mock.json";

  valent_test_ui_init (&argc, &argv, NULL);

  g_test_add ("/libvalent/ui/device-preferences-window/basic",
              ValentTestFixture, path,
              valent_test_fixture_init,
              test_device_preference_window_basic,
              valent_test_fixture_clear);

  g_test_add ("/libvalent/ui/device-preferences-window/navigation",
              ValentTestFixture, path,
              valent_test_fixture_init,
              test_device_preference_window_navigation,
              valent_test_fixture_clear);

  return g_test_run ();
}

