// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <libvalent-test.h>
#include <libvalent-ui.h>

#include "valent-plugin-row.h"


static void
test_plugin_row_basic (void)
{
  GtkWidget *row;
  PeasEngine *engine;
  PeasPluginInfo *info;

  char *plugin_context;
  PeasPluginInfo *plugin_info;
  GType plugin_type;

  engine = valent_get_engine ();
  info = peas_engine_get_plugin_info (engine, "mock");

  row = valent_plugin_row_new (info, "context");
  g_object_ref_sink (row);
  g_assert_true (VALENT_IS_PLUGIN_ROW (row));

  /* Properties */
  g_object_get (row,
                "plugin-context", &plugin_context,
                "plugin-info",    &plugin_info,
                "plugin-type",    &plugin_type,
                NULL);

  g_assert_cmpstr ("context", ==, plugin_context);
  g_assert_true (info == plugin_info);
  g_assert_true (PEAS_TYPE_EXTENSION == plugin_type);

  g_free (plugin_context);
  g_boxed_free (PEAS_TYPE_PLUGIN_INFO, plugin_info);

  g_object_unref (row);
}

int
main (int   argc,
      char *argv[])
{
  valent_test_ui_init (&argc, &argv, NULL);

  g_test_add_func ("/libvalent/ui/plugin-row",
                   test_plugin_row_basic);

  return g_test_run ();
}

