// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

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
  g_autoptr (GListStore) files_out = NULL;
  g_autoptr (GFile) file = NULL;

  file = g_file_new_for_uri ("resource:///tests/image.png");
  files = g_list_store_new (G_TYPE_FILE);
  g_list_store_append (files, file);


  VALENT_TEST_CHECK ("Window can be constructed");
  manager = valent_device_manager_get_default ();
  window = g_object_new (VALENT_TYPE_SHARE_TARGET_CHOOSER,
                         "files",          files,
                         NULL);
  g_object_add_weak_pointer (G_OBJECT (window), (gpointer)&window);

  g_object_get (window,
                "files",          &files_out,
                NULL);
  g_assert_true (files == files_out);

  VALENT_TEST_CHECK ("Window can be realized");
  gtk_window_present (window);
  valent_test_await_pending ();

  VALENT_TEST_CHECK ("Window adds devices");
  valent_application_plugin_startup (VALENT_APPLICATION_PLUGIN (manager));

  while ((service = valent_mock_channel_service_get_instance ()) == NULL)
    g_main_context_iteration (NULL, FALSE);

  valent_device_manager_refresh (manager);
  valent_test_await_pending ();

  VALENT_TEST_CHECK ("Window removes devices");
  valent_application_plugin_shutdown (VALENT_APPLICATION_PLUGIN (manager));
  valent_test_await_pending ();

  /* Wait for the window to close */
  gtk_window_destroy (window);
  valent_test_await_pending ();
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

