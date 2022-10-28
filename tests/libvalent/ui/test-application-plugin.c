// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>
#include <libvalent-core.h>
#include <libvalent-device.h>
#include <libvalent-test.h>
#include <libvalent-ui.h>


typedef struct
{
  GApplication        *application;
  ValentDeviceManager *manager;
  PeasExtension       *extension;
} ApplicationPluginFixture;

static void
application_fixture_set_up (ApplicationPluginFixture *fixture,
                            gconstpointer             user_data)
{
  PeasEngine *engine;
  PeasPluginInfo *plugin_info;

  engine = valent_get_plugin_engine ();
  plugin_info = peas_engine_get_plugin_info (engine, "mock");

  fixture->application = g_application_new ("ca.andyholmes.Valent.Tests",
                                            G_APPLICATION_FLAGS_NONE);
  fixture->manager = valent_device_manager_new_sync (NULL, NULL);
  fixture->extension = peas_engine_create_extension (engine,
                                                     plugin_info,
                                                     VALENT_TYPE_APPLICATION_PLUGIN,
                                                     "application",    fixture->application,
                                                     "device-manager", fixture->manager,
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
test_application_plugin_basic (ApplicationPluginFixture *fixture,
                               gconstpointer             user_data)
{
  ValentApplicationPlugin *plugin = VALENT_APPLICATION_PLUGIN (fixture->extension);
  g_autoptr (GApplication) application = NULL;
  g_autoptr (ValentDeviceManager) manager = NULL;
  PeasPluginInfo *plugin_info = NULL;

  /* Test properties */
  g_object_get (fixture->extension,
                "application",    &application,
                "device-manager", &manager,
                "plugin-info",    &plugin_info,
                NULL);

  g_assert_true (G_IS_APPLICATION (application));
  g_assert_true (VALENT_IS_DEVICE_MANAGER (manager));
  g_assert_nonnull (plugin_info);
  g_boxed_free (PEAS_TYPE_PLUGIN_INFO, plugin_info);

  application = valent_application_plugin_get_application (plugin);
  g_assert_true (G_IS_APPLICATION (application));

  manager = valent_application_plugin_get_device_manager (plugin);
  g_assert_true (VALENT_IS_DEVICE_MANAGER (manager));
}

int
main (int   argc,
      char *argv[])
{
  valent_test_init (&argc, &argv, NULL);

  g_test_add ("/libvalent/ui/application-plugin/basic",
              ApplicationPluginFixture, NULL,
              application_fixture_set_up,
              test_application_plugin_basic,
              application_fixture_tear_down);

  return g_test_run ();
}

