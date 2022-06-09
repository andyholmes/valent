// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <libvalent-test.h>
#include <libvalent-ui.h>


static void
test_share_plugin_preferences (void)
{
  PeasEngine *engine;
  PeasPluginInfo *info;
  PeasExtension *prefs;
  g_autofree char *device_id = NULL;

  engine = valent_get_engine ();
  info = peas_engine_get_plugin_info (engine, "share");
  prefs = peas_engine_create_extension (engine,
                                        info,
                                        VALENT_TYPE_DEVICE_PREFERENCES_PAGE,
                                        "device-id", "test-device",
                                        NULL);
  g_object_ref_sink (prefs);

  g_object_get (prefs, "device-id", &device_id, NULL);
  g_assert_cmpstr (device_id, ==, "test-device");

  g_object_unref (prefs);
}

static void
test_share_plugin_download_folder (void)
{
  PeasEngine *engine;
  PeasPluginInfo *info;
  PeasExtension *prefs;

  g_test_skip ("Settings schema 'org.gtk.gtk4.Settings.FileChooser' is not installed");
  return;

  engine = valent_get_engine ();
  info = peas_engine_get_plugin_info (engine, "share");
  prefs = peas_engine_create_extension (engine,
                                        info,
                                        VALENT_TYPE_DEVICE_PREFERENCES_PAGE,
                                        "device-id", "test-device",
                                        NULL);
  g_object_ref_sink (prefs);

  while (g_main_context_iteration (NULL, FALSE))
    continue;

  /* FileChooser Dialog */
  gtk_widget_activate_action (GTK_WIDGET (prefs),
                              "preferences.select-download-folder",
                              NULL);

  while (g_main_context_iteration (NULL, FALSE))
    continue;

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

