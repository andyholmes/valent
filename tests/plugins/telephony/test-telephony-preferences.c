// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <valent.h>
#include <libvalent-test.h>


static void
test_telephony_plugin_preferences (void)
{
  PeasEngine *engine;
  PeasPluginInfo *info;
  GObject *prefs;

  VALENT_TEST_CHECK ("Plugin can be constructed");
  engine = valent_get_plugin_engine ();
  info = peas_engine_get_plugin_info (engine, "telephony");
  prefs = peas_engine_create_extension (engine,
                                        info,
                                        VALENT_TYPE_DEVICE_PREFERENCES_GROUP,
                                        NULL);
  g_object_ref_sink (prefs);
  g_object_unref (prefs);
}

int
main (int   argc,
      char *argv[])
{
  valent_test_ui_init (&argc, &argv, NULL);

  g_test_add_func ("/plugins/telephony/preferences",
                   test_telephony_plugin_preferences);

  return g_test_run ();
}

