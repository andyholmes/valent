// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <valent.h>
#include <libvalent-test.h>


static void
test_share_plugin_preferences (void)
{
  PeasEngine *engine;
  PeasPluginInfo *info;
  GObject *prefs;

  VALENT_TEST_CHECK ("Plugin can be constructed");
  engine = valent_get_plugin_engine ();
  info = peas_engine_get_plugin_info (engine, "share");
  prefs = peas_engine_create_extension (engine,
                                        info,
                                        VALENT_TYPE_DEVICE_PREFERENCES_GROUP,
                                        NULL);
  g_object_ref_sink (prefs);
  g_object_unref (prefs);
}

static void
test_share_plugin_download_folder (void)
{
  PeasEngine *engine;
  PeasPluginInfo *info;
  GObject *prefs;

  g_test_skip ("Settings schema 'org.gtk.gtk4.Settings.FileChooser' is not installed");
  return;

  engine = valent_get_plugin_engine ();
  info = peas_engine_get_plugin_info (engine, "share");
  prefs = peas_engine_create_extension (engine,
                                        info,
                                        VALENT_TYPE_DEVICE_PREFERENCES_GROUP,
                                        NULL);
  g_object_ref_sink (prefs);
  valent_test_await_pending ();

  /* FileChooser Dialog */
  gtk_widget_activate_action (GTK_WIDGET (prefs),
                              "preferences.select-download-folder",
                              NULL);
  valent_test_await_pending ();

  g_object_unref (prefs);
}

int
main (int   argc,
      char *argv[])
{
  valent_test_ui_init (&argc, &argv, NULL);

  g_test_add_func ("/plugins/share/preferences",
                   test_share_plugin_preferences);

  g_test_add_func ("/plugins/share/select-download-folder",
                   test_share_plugin_download_folder);

  return g_test_run ();
}

