// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <valent.h>
#include <libvalent-test.h>


static void
test_device_preferences_group_basic (void)
{
  PeasEngine *engine;
  PeasPluginInfo *info;
  GObject *prefs;
  PeasPluginInfo *plugin_info = NULL;
  g_autoptr (GSettings) settings = NULL;

  engine = valent_get_plugin_engine ();
  info = peas_engine_get_plugin_info (engine, "mock");
  prefs = peas_engine_create_extension (engine,
                                        info,
                                        VALENT_TYPE_DEVICE_PREFERENCES_GROUP,
                                        NULL);
  g_object_ref_sink (prefs);

  g_object_get (prefs,
                "plugin-info", &plugin_info,
                "settings",    &settings,
                NULL);
  g_assert_true (plugin_info == info);
  g_boxed_free (PEAS_TYPE_PLUGIN_INFO, plugin_info);
  g_assert_false (G_IS_SETTINGS (settings));

  g_object_unref (prefs);
}

int
main (int   argc,
      char *argv[])
{
  valent_test_ui_init (&argc, &argv, NULL);

  g_test_add_func ("/libvalent/ui/device-preferences-group/basic",
                   test_device_preferences_group_basic);

  return g_test_run ();
}

