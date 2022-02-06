// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <libvalent-test.h>
#include <libvalent-ui.h>

#include "valent-window.h"


static ValentDeviceManager *
init_manager (void)
{
  g_autofree char *path = NULL;
  g_autoptr (ValentData) data = NULL;
  g_autoptr (JsonNode) packets = NULL;
  JsonNode *identity;
  g_autofree char *identity_json = NULL;
  g_autofree char *identity_path = NULL;

  /* Copy the mock device configuration */
  data = valent_data_new (NULL, NULL);
  path = g_build_filename (valent_data_get_config_path (data),
                           "test-device",
                           NULL);
  g_mkdir_with_parents (path, 0700);

  packets = valent_test_load_json (TEST_DATA_DIR"core.json");
  identity = json_object_get_member (json_node_get_object (packets), "identity");
  identity_json = json_to_string (identity, TRUE);
  identity_path = g_build_filename (path, "identity.json", NULL);
  g_file_set_contents (identity_path, identity_json, -1, NULL);

  return valent_device_manager_new_sync (data, NULL, NULL);
}

static void
test_window_basic (void)
{
  g_autoptr (ValentDeviceManager) manager = NULL;
  ValentDevice *device;
  GtkWindow *window;
  gpointer data;

  manager = init_manager ();

  window = g_object_new (VALENT_TYPE_WINDOW,
                         "device-manager", manager,
                         NULL);
  g_assert_nonnull (window);

  /* Properties */
  g_object_get (window,
                "device-manager", &data,
                NULL);
  g_assert_true (manager == data);
  g_object_unref (data);

  /* Remove Device */
  device = valent_device_manager_get_device (manager, "test-device");
  g_object_notify (G_OBJECT (device), "paired");

  g_clear_pointer (&window, gtk_window_destroy);
}

int
main (int argc,
     char *argv[])
{
  valent_test_ui_init (&argc, &argv, NULL);

  g_test_add_func ("/libvalent/ui/window",
                   test_window_basic);

  return g_test_run ();
}

