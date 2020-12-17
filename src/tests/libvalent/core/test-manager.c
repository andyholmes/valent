#include <gio/gio.h>
#include <libvalent-core.h>
#include <libvalent-test.h>


typedef struct
{
  GMainLoop           *loop;
  ValentManager *manager;
} ManagerFixture;

static void
on_device_added (ValentManager  *manager,
                 ValentDevice   *device,
                 ManagerFixture *fixture)
{
  g_signal_handlers_disconnect_by_func (manager, on_device_added, fixture);
  g_main_loop_quit (fixture->loop);
}

static void
load_cb (ValentManager  *manager,
         GAsyncResult   *result,
         ManagerFixture *fixture)
{
  GError *error = NULL;

  valent_manager_load_finish (manager, result, &error);
  g_assert_no_error (error);

  g_main_loop_quit (fixture->loop);
}

static void
manager_fixture_set_up (ManagerFixture *fixture,
                        gconstpointer   user_data)
{
  g_autofree char *path = NULL;
  g_autoptr (ValentData) data = NULL;
  g_autoptr (JsonNode) packets = NULL;
  JsonNode *identity;
  g_autofree char *identity_json = NULL;
  g_autofree char *identity_path = NULL;

  fixture->loop = g_main_loop_new (NULL, FALSE);
  fixture->manager = valent_manager_get_default ();

  /* Copy the mock device configuration */
  g_object_get (fixture->manager, "data", &data, NULL);
  path = g_build_filename (valent_data_get_config_path (data),
                           "test-device",
                           NULL);
  g_mkdir_with_parents (path, 0700);

  packets = valent_test_load_json (TEST_DATA_DIR"core.json");
  identity = json_object_get_member (json_node_get_object (packets), "identity");
  identity_json = json_to_string (identity, TRUE);
  identity_path = g_build_filename (path, "identity.json", NULL);
  g_file_set_contents (identity_path, identity_json, -1, NULL);
}

static void
manager_fixture_tear_down (ManagerFixture *fixture,
                           gconstpointer   user_data)
{
  valent_manager_stop (fixture->manager);

  v_assert_finalize_object (fixture->manager);
  g_clear_pointer (&fixture->loop, g_main_loop_unref);
}

static void
test_manager_load (ManagerFixture *fixture,
                   gconstpointer   user_data)
{
  g_autoptr (GPtrArray) devices = NULL;
  ValentDevice *device;

  /* Load cached devices */
  valent_manager_load_async (fixture->manager,
                             NULL,
                             (GAsyncReadyCallback)load_cb,
                             fixture);
  g_main_loop_run (fixture->loop);

  /* Loads devices from cache */
  devices = valent_manager_get_devices (fixture->manager);
  g_assert_cmpint (devices->len, ==, 1);

  device = valent_manager_get_device (fixture->manager, "test-device");
  g_assert_true (VALENT_IS_DEVICE (device));
}

static void
test_manager_start (ManagerFixture *fixture,
                    gconstpointer   user_data)
{
  g_autoptr (GPtrArray) devices = NULL;
  ValentDevice *device;

  /* Wait until the device is loaded */
  g_signal_connect (fixture->manager,
                    "device-added",
                    G_CALLBACK (on_device_added),
                    fixture);

  valent_manager_start (fixture->manager);
  g_main_loop_run (fixture->loop);

  while (g_main_context_iteration (NULL, FALSE))
    continue;

  devices = valent_manager_get_devices (fixture->manager);
  g_assert_cmpint (devices->len, ==, 1);

  device = valent_manager_get_device (fixture->manager, "test-device");
  g_assert_true (VALENT_IS_DEVICE (device));

  valent_manager_stop (fixture->manager);
}

static void
test_manager_management (ManagerFixture *fixture,
                         gconstpointer   user_data)
{
  ValentChannelService *service;
  ValentDevice *device;
  GPtrArray *devices;

  /* Wait until the device is loaded */
  g_signal_connect (fixture->manager,
                    "device-added",
                    G_CALLBACK (on_device_added),
                    fixture);

  valent_manager_start (fixture->manager);
  g_main_loop_run (fixture->loop);

  /* Loads devices from config directory */
  devices = valent_manager_get_devices (fixture->manager);
  g_assert_cmpint (devices->len, ==, 1);
  g_clear_pointer (&devices, g_ptr_array_unref);

  /* Removes unpaired devices automatically, when they disconnect */
  device = valent_manager_get_device (fixture->manager, "test-device");
  g_object_notify (G_OBJECT (device), "connected");

  devices = valent_manager_get_devices (fixture->manager);
  g_assert_cmpint (devices->len, ==, 0);
  g_clear_pointer (&devices, g_ptr_array_unref);

  /* Creates devices for channels */
  while ((service = valent_test_channel_service_get_instance ()) == NULL)
    g_main_context_iteration (NULL, FALSE);

  valent_manager_identify (fixture->manager, NULL);

  devices = valent_manager_get_devices (fixture->manager);
  g_assert_cmpint (devices->len, ==, 1);
  g_clear_pointer (&devices, g_ptr_array_unref);

  /* Removes devices when stopped */
  valent_manager_stop (fixture->manager);

  devices = valent_manager_get_devices (fixture->manager);
  g_assert_cmpint (devices->len, ==, 0);
  g_clear_pointer (&devices, g_ptr_array_unref);
}

static void
test_manager_dbus (ManagerFixture *fixture,
                   gconstpointer   user_data)
{
  g_autoptr (GDBusConnection) connection = NULL;

  connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
  g_signal_connect (fixture->manager,
                    "device-added",
                    G_CALLBACK (on_device_added),
                    fixture);

  /* Exports devices as they're added */
  valent_manager_export (fixture->manager, connection);
  valent_manager_load_async (fixture->manager, NULL, NULL, NULL);
  g_main_loop_run (fixture->loop);
  valent_manager_unexport (fixture->manager);

  /* Export devices already managed */
  valent_manager_export (fixture->manager, connection);
  valent_manager_unexport (fixture->manager);
}

static void
test_manager_dispose (ManagerFixture *fixture,
                      gconstpointer   user_data)
{
  PeasEngine *engine;
  ValentChannelService *service = NULL;

  /* Wait until the device is loaded */
  g_signal_connect (fixture->manager,
                    "device-added",
                    G_CALLBACK (on_device_added),
                    fixture);

  valent_manager_start (fixture->manager);
  g_main_loop_run (fixture->loop);

  /* Wait for the channel service */
  while ((service = valent_test_channel_service_get_instance ()) == NULL)
    g_main_context_iteration (NULL, FALSE);

  engine = valent_get_engine ();
  peas_engine_unload_plugin (engine, peas_engine_get_plugin_info (engine, "test"));

  while (g_main_context_iteration (NULL, FALSE))
    continue;

  service = valent_test_channel_service_get_instance ();
  g_assert_null (service);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, G_TEST_OPTION_ISOLATE_DIRS, NULL);

  g_test_add ("/core/manager/load",
              ManagerFixture, NULL,
              manager_fixture_set_up,
              test_manager_load,
              manager_fixture_tear_down);

  g_test_add ("/core/manager/start",
              ManagerFixture, NULL,
              manager_fixture_set_up,
              test_manager_start,
              manager_fixture_tear_down);

  g_test_add ("/core/manager/management",
              ManagerFixture, NULL,
              manager_fixture_set_up,
              test_manager_management,
              manager_fixture_tear_down);

#if VALENT_TEST_DBUS
  g_test_add ("/core/manager/dbus",
              ManagerFixture, NULL,
              manager_fixture_set_up,
              test_manager_dbus,
              manager_fixture_tear_down);
#endif

  g_test_add ("/core/manager/dispose",
              ManagerFixture, NULL,
              manager_fixture_set_up,
              test_manager_dispose,
              manager_fixture_tear_down);

  return g_test_run ();
}

