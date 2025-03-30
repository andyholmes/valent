// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>
#include <valent.h>
#include <libvalent-test.h>

static void
test_gnome_application (void)
{
  GApplication *application = NULL;
  GObject *extension = NULL;
  PeasEngine *engine;
  PeasPluginInfo *plugin_info;
  g_autoptr (ValentContext) context = NULL;

  engine = valent_get_plugin_engine ();
  plugin_info = peas_engine_get_plugin_info (engine, "gnome");
  context = valent_context_new (NULL, "plugin", "gnome");

  VALENT_TEST_CHECK ("Application can be constructed");
  application = g_application_new ("ca.andyholmes.Valent.Tests",
                                   G_APPLICATION_DEFAULT_FLAGS);

  VALENT_TEST_CHECK ("Application plugin can be constructed");
  extension = peas_engine_create_extension (engine,
                                            plugin_info,
                                            VALENT_TYPE_APPLICATION_PLUGIN,
                                            "iri",     "urn:valent:application:gnome",
                                            // FIXME: root source
                                            "source",  NULL,
                                            "context", context,
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

  g_test_add_func ("/plugins/gnome/application",
                   test_gnome_application);

  return g_test_run ();
}

