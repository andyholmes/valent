// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <libvalent-test.h>
#include <libvalent-ui.h>

#include "valent-plugin-group.h"


static void
test_plugin_group_basic (void)
{
  GtkWidget *group;
  PeasEngine *engine;
  PeasPluginInfo *info;

  char *plugin_context;
  GType plugin_type;

  group = g_object_new (VALENT_TYPE_PLUGIN_GROUP,
                        "plugin-context", "mock-context",
                        "plugin-type",    VALENT_TYPE_DEVICE_PLUGIN,
                        NULL);
  g_object_ref_sink (group);
  g_assert_true (VALENT_IS_PLUGIN_GROUP (group));

  /* Properties */
  g_object_get (group,
                "plugin-context", &plugin_context,
                "plugin-type",    &plugin_type,
                NULL);

  g_assert_cmpstr ("mock-context", ==, plugin_context);
  g_assert_true (VALENT_TYPE_DEVICE_PLUGIN == plugin_type);

  g_free (plugin_context);

  /* Unload the plugin */
  engine = valent_get_engine ();
  info = peas_engine_get_plugin_info (engine, "mock");
  peas_engine_unload_plugin (engine, info);

  g_object_unref (group);
}

int
main (int   argc,
      char *argv[])
{
  valent_test_ui_init (&argc, &argv, NULL);

  g_test_add_func ("/libvalent/ui/plugin-group",
                   test_plugin_group_basic);

  return g_test_run ();
}

