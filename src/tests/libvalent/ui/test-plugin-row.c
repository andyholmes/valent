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

  char *device_id;
  PeasPluginInfo *plugin_info;

  engine = valent_get_engine ();
  info = peas_engine_get_plugin_info (engine, "mock");

  row = g_object_new (VALENT_TYPE_PLUGIN_ROW,
                      "device-id",   "mock-context",
                      "plugin-info", info,
                      NULL);
  g_object_ref_sink (row);
  g_assert_true (VALENT_IS_PLUGIN_ROW (row));

  /* Properties */
  g_object_get (row,
                "device-id",   &device_id,
                "plugin-info", &plugin_info,
                NULL);

  g_assert_cmpstr ("mock-context", ==, device_id);
  g_assert_true (info == plugin_info);

  g_free (device_id);
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

