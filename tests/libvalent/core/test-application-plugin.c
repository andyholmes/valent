// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>
#include <valent.h>
#include <libvalent-test.h>


typedef struct
{
  GApplication *application;
  GObject      *extension;
} ApplicationPluginFixture;

static void
application_fixture_set_up (ApplicationPluginFixture *fixture,
                            gconstpointer             user_data)
{
  PeasEngine *engine;
  PeasPluginInfo *plugin_info;

  engine = valent_get_plugin_engine ();
  plugin_info = peas_engine_get_plugin_info (engine, "mock");

  VALENT_TEST_CHECK ("Plugin can be constructed");
  fixture->application = g_application_new ("ca.andyholmes.Valent.Tests",
                                            G_APPLICATION_DEFAULT_FLAGS);
  fixture->extension = peas_engine_create_extension (engine,
                                                     plugin_info,
                                                     VALENT_TYPE_APPLICATION_PLUGIN,
                                                     "object", fixture->application,
                                                     NULL);
}

static void
application_fixture_tear_down (ApplicationPluginFixture *fixture,
                               gconstpointer             user_data)
{
  v_await_finalize_object (fixture->extension);
  v_await_finalize_object (fixture->application);
}

static void
test_application_plugin_basic (ApplicationPluginFixture *fixture,
                               gconstpointer             user_data)
{
  ValentApplicationPlugin *plugin = VALENT_APPLICATION_PLUGIN (fixture->extension);
  g_autoptr (GApplication) application = NULL;
  PeasPluginInfo *plugin_info = NULL;

  VALENT_TEST_CHECK ("GObject properties function correctly");
  g_object_get (fixture->extension,
                "object",      &application,
                "plugin-info", &plugin_info,
                NULL);

  g_assert_true (G_IS_APPLICATION (application));
  g_assert_nonnull (plugin_info);
  g_boxed_free (PEAS_TYPE_PLUGIN_INFO, plugin_info);

  application = valent_extension_get_object (VALENT_EXTENSION (plugin));
  g_assert_true (G_IS_APPLICATION (application));
}

int
main (int   argc,
      char *argv[])
{
  valent_test_init (&argc, &argv, NULL);

  g_test_add ("/libvalent/core/application-plugin/basic",
              ApplicationPluginFixture, NULL,
              application_fixture_set_up,
              test_application_plugin_basic,
              application_fixture_tear_down);

  return g_test_run ();
}

