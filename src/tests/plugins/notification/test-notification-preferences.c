// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <libvalent-test.h>
#include <libvalent-ui.h>


static void
test_notification_plugin_preferences (void)
{
  PeasEngine *engine;
  PeasPluginInfo *info;
  PeasExtension *prefs;
  g_autofree char *plugin_context = NULL;

  engine = valent_get_engine ();
  info = peas_engine_get_plugin_info (engine, "notification");
  prefs = peas_engine_create_extension (engine,
                                        info,
                                        VALENT_TYPE_PLUGIN_PREFERENCES,
                                        "plugin-context", "test-device",
                                        NULL);
  g_object_ref_sink (prefs);

  g_object_get (prefs, "plugin-context", &plugin_context, NULL);
  g_assert_cmpstr (plugin_context, ==, "test-device");

  g_object_unref (prefs);
}

int
main (int   argc,
      char *argv[])
{
  valent_test_ui_init (&argc, &argv, NULL);

  g_test_add_func ("/plugins/notification/preferences",
                   test_notification_plugin_preferences);

  return g_test_run ();
}

