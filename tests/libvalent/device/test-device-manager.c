// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>
#include <valent.h>
#include <libvalent-test.h>

#include "valent-mock-channel.h"
#include "valent-mock-channel-service.h"

#define TEST_OBJECT_PATH "/ca/andyholmes/Valent/Test"
#define DEVICE_INTERFACE "ca.andyholmes.Valent.Device"


typedef struct
{
  ValentDeviceManager  *manager;
  ValentChannelService *service;
  ValentDevice         *device;
  gpointer              data;
} ManagerFixture;


static void
manager_fixture_set_up (ManagerFixture *fixture,
                        gconstpointer   user_data)
{
  ValentResource *source = NULL;
  g_autoptr (GFile) cache = NULL;
  g_autoptr (JsonNode) state = NULL;
  g_autofree char *state_json = NULL;
  g_autofree char *state_path = NULL;

  /* Copy the mock device configuration */
  source = valent_data_source_get_local_default ();
  cache = valent_data_source_get_cache_file (VALENT_DATA_SOURCE (source), "..");

  state = valent_test_load_json ("core-state.json");
  state_json = json_to_string (state, TRUE);
  state_path = g_build_filename (g_file_peek_path (cache), "devices.json", NULL);
  g_file_set_contents (state_path, state_json, -1, NULL);

  fixture->manager = valent_device_manager_get_default ();
}

static void
manager_fixture_tear_down (ManagerFixture *fixture,
                           gconstpointer   user_data)
{
  while (valent_mock_channel_service_get_instance () != NULL)
    g_main_context_iteration (NULL, FALSE);

  v_await_finalize_object (fixture->manager);
}

static void
on_devices_changed (GListModel     *list,
                    unsigned int    position,
                    unsigned int    removed,
                    unsigned int    added,
                    ManagerFixture *fixture)
{
  if (added == 1)
    {
      g_autoptr (ValentDevice) device = NULL;

      device = g_list_model_get_item (list, position);
      fixture->device = device;
    }

  if (removed == 1)
    {
      fixture->device = NULL;
    }
}

static void
test_manager_management (ManagerFixture *fixture,
                         gconstpointer   user_data)
{
  unsigned int n_devices = 0;

  g_signal_connect (fixture->manager,
                    "items-changed",
                    G_CALLBACK (on_devices_changed),
                    fixture);

  VALENT_TEST_CHECK ("Manager starts up with the application");
  valent_application_plugin_startup (VALENT_APPLICATION_PLUGIN (fixture->manager));
  valent_test_await_pointer (&fixture->device);

  VALENT_TEST_CHECK ("Manager adds devices from the cache when started");
  n_devices = g_list_model_get_n_items (G_LIST_MODEL (fixture->manager));
  g_assert_cmpuint (n_devices, ==, 1);

  VALENT_TEST_CHECK ("Manager removes unpaired devices when they disconnect");
  g_object_notify (G_OBJECT (fixture->device), "state");
  g_assert_false (VALENT_IS_DEVICE (fixture->device));

  n_devices = g_list_model_get_n_items (G_LIST_MODEL (fixture->manager));
  g_assert_cmpuint (n_devices, ==, 0);

  VALENT_TEST_CHECK ("Manager adds devices from new channels");
  valent_device_manager_refresh (fixture->manager);
  g_assert_true (VALENT_IS_DEVICE (fixture->device));

  n_devices = g_list_model_get_n_items (G_LIST_MODEL (fixture->manager));
  g_assert_cmpuint (n_devices, ==, 1);

  VALENT_TEST_CHECK ("Manager retains paired devices when they disconnect");
  g_object_notify (G_OBJECT (fixture->device), "state");
  g_assert_true (VALENT_IS_DEVICE (fixture->device));

  VALENT_TEST_CHECK ("Manager shuts down with the application");
  valent_application_plugin_shutdown (VALENT_APPLICATION_PLUGIN (fixture->manager));
  valent_test_await_nullptr (&fixture->device);

  g_signal_handlers_disconnect_by_data (fixture->manager, fixture);
}

static void
manager_finish (GObject             *object,
                GAsyncResult        *result,
                GDBusObjectManager **manager)
{
  GError *error = NULL;

  *manager = g_dbus_object_manager_client_new_finish (result, &error);
  g_assert_no_error (error);
}

static void
test_manager_dbus (ManagerFixture *fixture,
                   gconstpointer   user_data)
{
  g_autoptr (GDBusConnection) connection = NULL;
  g_autoptr (GDBusObjectManager) manager = NULL;
  g_autolist (GDBusObject) objects = NULL;
  g_autoptr (GDBusInterface) interface = NULL;
  g_autoptr (GDBusActionGroup) actions = NULL;
  g_auto (GStrv) action_names = NULL;
  g_autoptr (GDBusMenuModel) menu = NULL;
  const char *unique_name;
  const char *object_path;

  g_signal_connect (fixture->manager,
                    "items-changed",
                    G_CALLBACK (on_devices_changed),
                    fixture);

  VALENT_TEST_CHECK ("Manager starts up with the application");
  valent_application_plugin_startup (VALENT_APPLICATION_PLUGIN (fixture->manager));
  valent_test_await_pointer (&fixture->device);

  VALENT_TEST_CHECK ("Manager can be exported on D-Bus");
  connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
  valent_application_plugin_dbus_register (VALENT_APPLICATION_PLUGIN (fixture->manager),
                                           connection,
                                           TEST_OBJECT_PATH,
                                           NULL);

  unique_name = g_dbus_connection_get_unique_name (connection);
  g_dbus_object_manager_client_new (connection,
                                    G_DBUS_OBJECT_MANAGER_CLIENT_FLAGS_NONE,
                                    unique_name,
                                    TEST_OBJECT_PATH,
                                    NULL, NULL, NULL,
                                    NULL,
                                    (GAsyncReadyCallback)manager_finish,
                                    &manager);
  valent_test_await_pointer (&manager);

  VALENT_TEST_CHECK ("Manager exports devices on D-Bus");
  objects = g_dbus_object_manager_get_objects (manager);
  g_assert_cmpuint (g_list_length (objects), ==, 1);

  object_path = g_dbus_object_get_object_path (objects->data);
  interface = g_dbus_object_get_interface (objects->data, DEVICE_INTERFACE);
  g_assert_nonnull (interface);

  g_object_notify (G_OBJECT (fixture->device), "icon-name");
  valent_test_await_signal (interface, "g-properties-changed");

  VALENT_TEST_CHECK ("Manager exports action group on D-Bus");
  actions = g_dbus_action_group_get (connection, unique_name, object_path);

  action_names = g_action_group_list_actions (G_ACTION_GROUP (actions));
  g_clear_pointer (&action_names, g_strfreev);
  valent_test_await_signal (actions, "action-added");

  action_names = g_action_group_list_actions (G_ACTION_GROUP (actions));
  g_assert_cmpuint (g_strv_length (action_names), >, 0);
  g_clear_pointer (&action_names, g_strfreev);

  VALENT_TEST_CHECK ("Manager exports menu model on D-Bus");
  menu = g_dbus_menu_model_get (connection, unique_name, object_path);

  VALENT_TEST_CHECK ("Manager unexports devices from D-Bus");
  valent_application_plugin_dbus_unregister (VALENT_APPLICATION_PLUGIN (fixture->manager),
                                             connection,
                                             TEST_OBJECT_PATH);
  valent_test_await_signal (manager, "object-removed");

  VALENT_TEST_CHECK ("Manager shuts down with the application");
  valent_application_plugin_shutdown (VALENT_APPLICATION_PLUGIN (fixture->manager));
  valent_test_await_nullptr (&fixture->device);

  g_signal_handlers_disconnect_by_data (fixture->manager, fixture);
}

static void
test_manager_dispose (ManagerFixture *fixture,
                      gconstpointer   user_data)
{
  PeasEngine *engine;
  ValentChannelService *service = NULL;
  g_autoptr (GSettings) settings = NULL;

  VALENT_TEST_CHECK ("Manager starts up with the application");
  valent_application_plugin_startup (VALENT_APPLICATION_PLUGIN (fixture->manager));

  while ((service = valent_mock_channel_service_get_instance ()) == NULL)
    g_main_context_iteration (NULL, FALSE);

  VALENT_TEST_CHECK ("Manager stops channel services when a plugin is disabled");
  settings = valent_test_mock_settings ("network");
  g_settings_set_boolean (settings, "enabled", FALSE);

  while ((service = valent_mock_channel_service_get_instance ()) != NULL)
    g_main_context_iteration (NULL, FALSE);

  VALENT_TEST_CHECK ("Manager starts channel services when a plugin is enabled");
  g_settings_set_boolean (settings, "enabled", TRUE);

  while ((service = valent_mock_channel_service_get_instance ()) == NULL)
    g_main_context_iteration (NULL, FALSE);

  VALENT_TEST_CHECK ("Manager stops channel services when a plugin is unloaded");
  engine = valent_get_plugin_engine ();
  peas_engine_unload_plugin (engine, peas_engine_get_plugin_info (engine, "mock"));

  while ((service = valent_mock_channel_service_get_instance ()) != NULL)
    g_main_context_iteration (NULL, FALSE);

  VALENT_TEST_CHECK ("Manager starts channel services when a plugin is loaded");
  peas_engine_load_plugin (engine, peas_engine_get_plugin_info (engine, "mock"));

  while ((service = valent_mock_channel_service_get_instance ()) == NULL)
    g_main_context_iteration (NULL, FALSE);

  VALENT_TEST_CHECK ("Manager shuts down with the application");
  valent_application_plugin_shutdown (VALENT_APPLICATION_PLUGIN (fixture->manager));
  valent_test_await_nullptr (&fixture->device);

  g_signal_handlers_disconnect_by_data (fixture->manager, fixture);
}

int
main (int   argc,
      char *argv[])
{
  valent_test_init (&argc, &argv, NULL);

  g_test_add ("/libvalent/device/device-manager/management",
              ManagerFixture, NULL,
              manager_fixture_set_up,
              test_manager_management,
              manager_fixture_tear_down);

  g_test_add ("/libvalent/device/device-manager/dbus",
              ManagerFixture, NULL,
              manager_fixture_set_up,
              test_manager_dbus,
              manager_fixture_tear_down);

  g_test_add ("/libvalent/device/device-manager/dispose",
              ManagerFixture, NULL,
              manager_fixture_set_up,
              test_manager_dispose,
              manager_fixture_tear_down);

  return g_test_run ();
}

