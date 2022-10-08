// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>
#include <libvalent-core.h>
#include <libvalent-test.h>

#include "valent-mock-channel.h"
#include "valent-mock-channel-service.h"

#define TEST_OBJECT_PATH "/ca/andyholmes/Valent/Test"
#define DEVICE_INTERFACE "ca.andyholmes.Valent.Device"


typedef struct
{
  GMainLoop            *loop;
  ValentDeviceManager  *manager;
  ValentChannelService *service;
  ValentDevice         *device;
  gpointer              data;
} ManagerFixture;


static void
manager_fixture_set_up (ManagerFixture *fixture,
                        gconstpointer   user_data)
{
  const char *path = NULL;
  g_autoptr (ValentData) data = NULL;
  g_autoptr (JsonNode) state = NULL;
  g_autofree char *state_json = NULL;
  g_autofree char *state_path = NULL;

  /* Copy the mock device configuration */
  data = valent_data_new (NULL, NULL);
  path = valent_data_get_cache_path (data);
  g_mkdir_with_parents (path, 0700);

  state = valent_test_load_json (TEST_DATA_DIR"/core-state.json");
  state_json = json_to_string (state, TRUE);
  state_path = g_build_filename (path, "devices.json", NULL);
  g_file_set_contents (state_path, state_json, -1, NULL);

  /* Init the manager */
  fixture->loop = g_main_loop_new (NULL, FALSE);
  fixture->manager = valent_device_manager_new_sync (NULL, NULL);
}

static void
manager_fixture_tear_down (ManagerFixture *fixture,
                           gconstpointer   user_data)
{
  valent_device_manager_stop (fixture->manager);

  while (valent_mock_channel_service_get_instance () != NULL)
    g_main_context_iteration (NULL, FALSE);

  v_await_finalize_object (fixture->manager);
  g_clear_pointer (&fixture->loop, g_main_loop_unref);
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
manager_new_cb (GObject      *object,
                GAsyncResult *result,
                gboolean     *done)
{
  g_autoptr (ValentDeviceManager) manager = NULL;
  g_autoptr (GError) error = NULL;

  manager = valent_device_manager_new_finish (result, &error);
  g_assert_no_error (error);
  g_assert_true (VALENT_IS_DEVICE_MANAGER (manager));

  if (done != NULL)
    *done = TRUE;
}

static void
test_manager_new (void)
{
  gboolean done = FALSE;

  valent_device_manager_new (NULL,
                             (GAsyncReadyCallback)manager_new_cb,
                             &done);

  while (!done)
    g_main_context_iteration (NULL, FALSE);
}

static void
test_manager_basic (ManagerFixture *fixture,
                    gconstpointer   user_data)
{
  g_autofree char *id = NULL;
  g_autofree char *name = NULL;

  /* Test properties */
  g_object_get (fixture->manager,
                "id",   &id,
                "name", &name,
                NULL);
  g_assert_nonnull (id);
  g_assert_cmpstr (name, ==, "Valent");
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

  /* Wait for the manager to start */
  valent_device_manager_start (fixture->manager);

  while (fixture->device == NULL)
    g_main_context_iteration (NULL, FALSE);

  /* Adds devices from the cache when started */
  n_devices = g_list_model_get_n_items (G_LIST_MODEL (fixture->manager));
  g_assert_cmpuint (n_devices, ==, 1);

  /* Removes unpaired devices that disconnect */
  g_object_notify (G_OBJECT (fixture->device), "state");
  g_assert_false (VALENT_IS_DEVICE (fixture->device));

  n_devices = g_list_model_get_n_items (G_LIST_MODEL (fixture->manager));
  g_assert_cmpuint (n_devices, ==, 0);

  /* Adds devices from channels */
  valent_device_manager_identify (fixture->manager, NULL);
  g_assert_true (VALENT_IS_DEVICE (fixture->device));

  n_devices = g_list_model_get_n_items (G_LIST_MODEL (fixture->manager));
  g_assert_cmpuint (n_devices, ==, 1);

  /* Retains paired devices that disconnect */
  g_object_notify (G_OBJECT (fixture->device), "state");
  g_assert_true (VALENT_IS_DEVICE (fixture->device));
}

static void
test_manager_identify_uri (ManagerFixture *fixture,
                           gconstpointer   user_data)
{
  g_signal_connect (fixture->manager,
                    "items-changed",
                    G_CALLBACK (on_devices_changed),
                    fixture);

  /* Wait for the manager to start */
  valent_device_manager_start (fixture->manager);

  while (fixture->device == NULL)
    g_main_context_iteration (NULL, FALSE);

  /* Drop the cached device */
  g_object_notify (G_OBJECT (fixture->device), "state");

  /* Forwards URIs to the correct service */
  valent_device_manager_identify (fixture->manager, "mock://127.0.0.1");
  g_assert_true (VALENT_IS_DEVICE (fixture->device));
}

static void
manager_finish (GObject        *object,
                GAsyncResult   *result,
                ManagerFixture *fixture)
{
  GError *error = NULL;

  fixture->data = g_dbus_object_manager_client_new_finish (result, &error);
  g_assert_no_error (error);

  g_main_loop_quit (fixture->loop);
}

static void
on_action_added (GActionGroup   *group,
                 char           *name,
                 ManagerFixture *fixture)
{
  g_main_loop_quit (fixture->loop);
}

static void
on_properties_changed (GDBusProxy     *proxy,
                       GVariant       *changed_properties,
                       GStrv           invalidated_properties,
                       ManagerFixture *fixture)
{
  fixture->data = proxy;
  g_main_loop_quit (fixture->loop);
}

static void
on_object_removed (GDBusObjectManager *manager,
                   GDBusObject        *object,
                   ManagerFixture     *fixture)
{
  g_main_loop_quit (fixture->loop);
}

static void
test_manager_dbus (ManagerFixture *fixture,
                   gconstpointer   user_data)
{
  ValentDevice *device;
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

  /* Wait for the manager to start */
  valent_device_manager_start (fixture->manager);

  while (fixture->device == NULL)
    g_main_context_iteration (NULL, FALSE);

  /* Exports current devices */
  connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
  valent_device_manager_export (fixture->manager, connection, TEST_OBJECT_PATH);

  unique_name = g_dbus_connection_get_unique_name (connection);
  g_dbus_object_manager_client_new (connection,
                                    G_DBUS_OBJECT_MANAGER_CLIENT_FLAGS_NONE,
                                    unique_name,
                                    TEST_OBJECT_PATH,
                                    NULL, NULL, NULL,
                                    NULL,
                                    (GAsyncReadyCallback)manager_finish,
                                    fixture);
  g_main_loop_run (fixture->loop);
  manager = g_steal_pointer (&fixture->data);

  /* Exports devices */
  objects = g_dbus_object_manager_get_objects (manager);
  g_assert_cmpuint (g_list_length (objects), ==, 1);

  object_path = g_dbus_object_get_object_path (objects->data);
  interface = g_dbus_object_get_interface (objects->data, DEVICE_INTERFACE);
  g_assert_nonnull (interface);

  g_signal_connect (interface,
                    "g-properties-changed",
                    G_CALLBACK (on_properties_changed),
                    fixture);

  device = valent_device_manager_get_device_by_id (fixture->manager, "test-device");
  g_object_notify (G_OBJECT (device), "type");
  g_main_loop_run (fixture->loop);

  g_assert_true (fixture->data == interface);
  fixture->data = NULL;

  g_clear_pointer (&objects, valent_object_list_free);

  /* Exports Actions */
  actions = g_dbus_action_group_get (connection, unique_name, object_path);

  g_signal_connect (actions,
                    "action-added",
                    G_CALLBACK (on_action_added),
                    fixture);
  action_names = g_action_group_list_actions (G_ACTION_GROUP (actions));
  g_clear_pointer (&action_names, g_strfreev);
  g_main_loop_run (fixture->loop);

  action_names = g_action_group_list_actions (G_ACTION_GROUP (actions));
  g_assert_cmpuint (g_strv_length (action_names), >, 0);
  g_clear_pointer (&action_names, g_strfreev);

  /* Exports Menus */
  menu = g_dbus_menu_model_get (connection, unique_name, object_path);

  /* Unexports devices */
  g_signal_connect (manager,
                    "object-removed",
                    G_CALLBACK (on_object_removed),
                    fixture);

  valent_device_manager_unexport (fixture->manager);
  g_main_loop_run (fixture->loop);
}

static void
test_manager_dispose (ManagerFixture *fixture,
                      gconstpointer   user_data)
{
  PeasEngine *engine;
  ValentChannelService *service = NULL;
  g_autoptr (GSettings) settings = NULL;

  /* Startup */
  valent_device_manager_start (fixture->manager);

  while ((service = valent_mock_channel_service_get_instance ()) == NULL)
    g_main_context_iteration (NULL, FALSE);

  /* Disable & Enabled channel service */
  settings = valent_component_create_settings ("network", "mock");

  g_settings_set_boolean (settings, "enabled", FALSE);

  while ((service = valent_mock_channel_service_get_instance ()) != NULL)
    g_main_context_iteration (NULL, FALSE);

  g_settings_set_boolean (settings, "enabled", TRUE);

  while ((service = valent_mock_channel_service_get_instance ()) == NULL)
    g_main_context_iteration (NULL, FALSE);

  /* Unload/Load plugin */
  engine = valent_get_plugin_engine ();
  peas_engine_unload_plugin (engine, peas_engine_get_plugin_info (engine, "mock"));

  while ((service = valent_mock_channel_service_get_instance ()) != NULL)
    g_main_context_iteration (NULL, FALSE);

  peas_engine_load_plugin (engine, peas_engine_get_plugin_info (engine, "mock"));

  while ((service = valent_mock_channel_service_get_instance ()) == NULL)
    g_main_context_iteration (NULL, FALSE);

  /* Shutdown */
  valent_device_manager_stop (fixture->manager);

  while ((service = valent_mock_channel_service_get_instance ()) != NULL)
    g_main_context_iteration (NULL, FALSE);
}

int
main (int   argc,
      char *argv[])
{
  valent_test_init (&argc, &argv, NULL);

  g_test_add_func ("/core/manager/new", test_manager_new);

  g_test_add ("/libvalent/core/device-manager/basic",
              ManagerFixture, NULL,
              manager_fixture_set_up,
              test_manager_basic,
              manager_fixture_tear_down);

  g_test_add ("/libvalent/core/device-manager/management",
              ManagerFixture, NULL,
              manager_fixture_set_up,
              test_manager_management,
              manager_fixture_tear_down);

  g_test_add ("/libvalent/core/device-manager/identify-uri",
              ManagerFixture, NULL,
              manager_fixture_set_up,
              test_manager_identify_uri,
              manager_fixture_tear_down);

  g_test_add ("/libvalent/core/device-manager/dbus",
              ManagerFixture, NULL,
              manager_fixture_set_up,
              test_manager_dbus,
              manager_fixture_tear_down);

  g_test_add ("/libvalent/core/device-manager/dispose",
              ManagerFixture, NULL,
              manager_fixture_set_up,
              test_manager_dispose,
              manager_fixture_tear_down);

  return g_test_run ();
}

