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

  window = g_object_new (VALENT_TYPE_PREFERENCES_WINDOW,
                        NULL);
  g_assert_true (VALENT_IS_PREFERENCES_WINDOW (window));

  /* Unload the plugin */
  engine = valent_get_engine ();
  peas_engine_unload_plugin (engine,
                             peas_engine_get_plugin_info (engine, "mock"));

  g_clear_pointer (&window, gtk_window_destroy);
}

int
main (int   argc,
      char *argv[])
{
  valent_test_ui_init (&argc, &argv, NULL);

  g_test_add_func ("/libvalent/ui/preferences-window",
                   test_preferences_window_basic);

  return g_test_run ();
}

