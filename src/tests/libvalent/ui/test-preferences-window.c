// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <libvalent-test.h>
#include <libvalent-ui.h>

#include "valent-preferences-window.h"


static void
test_preferences_window_basic (void)
{
  GtkWindow *window;
  PeasEngine *engine;
  PeasPluginInfo *info;

  window = g_object_new (VALENT_TYPE_PREFERENCES_WINDOW,
                        NULL);
  g_assert_true (VALENT_IS_PREFERENCES_WINDOW (window));

  /* Unload/Load the plugin */
  engine = valent_get_engine ();
  info = peas_engine_get_plugin_info (engine, "mock");
  peas_engine_unload_plugin (engine, info);
  peas_engine_load_plugin (engine, info);

  g_clear_pointer (&window, gtk_window_destroy);
}

static void
test_preferences_window_navigation (void)
{
  GtkWindow *window;

  window = g_object_new (VALENT_TYPE_PREFERENCES_WINDOW,
                        NULL);
  g_assert_true (VALENT_IS_PREFERENCES_WINDOW (window));

  g_object_add_weak_pointer (G_OBJECT (window), (gpointer)&window);
  gtk_window_present (window);

  while (g_main_context_iteration (NULL, FALSE))
    continue;

  /* Main -> Plugin */
  gtk_widget_activate_action (GTK_WIDGET (window), "win.page", "s", "mock");

  while (g_main_context_iteration (NULL, FALSE))
    continue;

  /* Plugin -> Previous */
  gtk_widget_activate_action (GTK_WIDGET (window), "win.previous", NULL);

  while (g_main_context_iteration (NULL, FALSE))
    continue;

  /* Main -> Previous (Close Preferences) */
  gtk_widget_activate_action (GTK_WIDGET (window), "win.previous", NULL);

  while (g_main_context_iteration (NULL, FALSE))
    continue;

  g_assert_null (window);
}

static void
test_preferences_window_rename (void)
{
  GtkWindow *window;

  window = g_object_new (VALENT_TYPE_PREFERENCES_WINDOW,
                        NULL);
  g_assert_true (VALENT_IS_PREFERENCES_WINDOW (window));

  gtk_window_present (window);

  while (g_main_context_iteration (NULL, FALSE))
    continue;

  /* Rename Dialog */
  gtk_widget_activate_action (GTK_WIDGET (window), "win.rename", NULL);

  while (g_main_context_iteration (NULL, FALSE))
    continue;

  g_clear_pointer (&window, gtk_window_destroy);
}

int
main (int   argc,
      char *argv[])
{
  valent_test_ui_init (&argc, &argv, NULL);

  g_test_add_func ("/libvalent/ui/preferences-window",
                   test_preferences_window_basic);

  g_test_add_func ("/libvalent/ui/preferences-window/navigation",
                   test_preferences_window_navigation);

  g_test_add_func ("/libvalent/ui/preferences-window/rename",
                   test_preferences_window_rename);

  return g_test_run ();
}

