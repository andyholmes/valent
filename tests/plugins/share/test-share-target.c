// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>
#include <valent.h>
#include <libvalent-test.h>


typedef struct
{
  GApplication        *application;
  ValentDeviceManager *manager;
  GObject             *extension;
} ApplicationPluginFixture;

static void
application_fixture_set_up (ApplicationPluginFixture *fixture,
                            gconstpointer             user_data)
{
  PeasEngine *engine;
  PeasPluginInfo *plugin_info;

  engine = valent_get_plugin_engine ();
  plugin_info = peas_engine_get_plugin_info (engine, "share");

  fixture->application = g_application_new ("ca.andyholmes.Valent.Tests",
                                            G_APPLICATION_DEFAULT_FLAGS);
  fixture->manager = valent_device_manager_get_default ();
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
  v_await_finalize_object (fixture->manager);
  v_await_finalize_object (fixture->application);
}

static void
test_share_target (ApplicationPluginFixture *fixture,
                               gconstpointer             user_data)
{
  ValentApplicationPlugin *plugin = VALENT_APPLICATION_PLUGIN (fixture->extension);
  g_autoptr (GApplication) application = NULL;

  VALENT_TEST_CHECK ("GObject properties function correctly");
  g_object_get (fixture->extension,
                "object", &application,
                NULL);
  g_assert_true (G_IS_APPLICATION (application));

  application = valent_extension_get_object (VALENT_EXTENSION (plugin));
  g_assert_true (G_IS_APPLICATION (application));
}

int
main (int   argc,
      char *argv[])
{
  valent_test_init (&argc, &argv, NULL);

  g_test_add ("/plugins/share/target",
              ApplicationPluginFixture, NULL,
              application_fixture_set_up,
              test_share_target,
              application_fixture_tear_down);

  return g_test_run ();
}

