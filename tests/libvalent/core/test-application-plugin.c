// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>
#include <valent.h>
#include <libvalent-test.h>

static void
test_application_plugin (void)
{
  GApplication *application = NULL;
  GObject *extension = NULL;
  PeasEngine *engine;
  PeasPluginInfo *plugin_info;

  VALENT_TEST_CHECK ("Application can be constructed");
  application = g_application_new ("ca.andyholmes.Valent.Tests",
                                   G_APPLICATION_DEFAULT_FLAGS);

  VALENT_TEST_CHECK ("Application plugin can be constructed");
  engine = valent_get_plugin_engine ();
  plugin_info = peas_engine_get_plugin_info (engine, "mock");
  extension = peas_engine_create_extension (engine,
                                            plugin_info,
                                            VALENT_TYPE_APPLICATION_PLUGIN,
                                            // FIXME: root source
                                            "source", NULL,
                                            NULL);

  v_await_finalize_object (extension);
  v_await_finalize_object (valent_device_manager_get_default ());
  v_await_finalize_object (application);
}

int
main (int   argc,
      char *argv[])
{
  valent_test_ui_init (&argc, &argv, NULL);

  g_test_add_func ("/libvalent/core/application-plugin",
                   test_application_plugin);

  return g_test_run ();
}

