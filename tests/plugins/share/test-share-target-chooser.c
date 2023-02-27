// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <valent.h>
#include <libvalent-test.h>

#include "valent-mock-channel.h"
#include "valent-mock-channel-service.h"
#include "valent-share-target-chooser.h"


static void
test_share_target_chooser (void)
{
  GtkWindow *window = NULL;
  ValentChannelService *service = NULL;
  g_autoptr (ValentDeviceManager) manager = NULL;
  g_autoptr (GListStore) files = NULL;
  g_autoptr (ValentDeviceManager) manager_out = NULL;
  g_autoptr (GListStore) files_out = NULL;
  g_autoptr (GFile) file = NULL;

  file = g_file_new_for_uri ("resource:///tests/image.png");
  files = g_list_store_new (G_TYPE_FILE);
  g_list_store_append (files, file);

  manager = valent_device_manager_new_sync (NULL, NULL);
  window = g_object_new (VALENT_TYPE_SHARE_TARGET_CHOOSER,
                         "device-manager", manager,
                         "files",          files,
                         NULL);

  g_object_get (window,
                "device-manager", &manager_out,
                "files",          &files_out,
                NULL);
  g_assert_true (manager == manager_out);
  g_assert_true (files == files_out);

  /* Wait for the window to open */
  gtk_window_present (window);

  while (g_main_context_iteration (NULL, FALSE))
    continue;

  /* Wait for the manager to start */
  valent_device_manager_start (manager);

  while ((service = valent_mock_channel_service_get_instance ()) == NULL)
    g_main_context_iteration (NULL, FALSE);

  /* ... */
  valent_device_manager_refresh (manager);

  while (g_main_context_iteration (NULL, FALSE))
    continue;

  valent_device_manager_stop (manager);

  while (g_main_context_iteration (NULL, FALSE))
    continue;

  /* Wait for the window to close */
  gtk_window_destroy (window);

  while (g_main_context_iteration (NULL, FALSE))
    continue;
}

int
main (int   argc,
      char *argv[])
{
  valent_test_ui_init (&argc, &argv, NULL);

  g_test_add_func ("/plugins/share/target-chooser",
                   test_share_target_chooser);

  return g_test_run ();
}

