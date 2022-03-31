// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <libvalent-test.h>
#include <libvalent-ui.h>

#include "valent-window.h"


typedef struct
{
  ValentDeviceManager *manager;
} TestWindowFixture;

static void
test_window_set_up (TestWindowFixture *fixture,
                    gconstpointer      user_data)
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

  fixture->manager = valent_device_manager_new_sync (data, NULL, NULL);
}

static void
test_window_tear_down (TestWindowFixture *fixture,
                       gconstpointer      user_data)
{
  g_clear_object (&fixture->manager);
}

static void
test_window_basic (TestWindowFixture *fixture,
                   gconstpointer      user_data)
{
  g_autoptr (ValentDeviceManager) manager = NULL;
  ValentDevice *device;
  GtkWindow *window;

  window = g_object_new (VALENT_TYPE_WINDOW,
                         "device-manager", fixture->manager,
                         NULL);
  g_assert_true (VALENT_IS_WINDOW (window));
  g_assert_nonnull (window);

  gtk_window_present (window);

  while (g_main_context_iteration (NULL, FALSE))
    continue;

  /* Properties */
  g_object_get (window,
                "device-manager", &manager,
                NULL);
  g_assert_true (fixture->manager == manager);
  g_clear_object (&manager);

  /* Remove Device */
  device = valent_device_manager_get_device (fixture->manager, "test-device");
  g_object_notify (G_OBJECT (device), "state");

  g_clear_pointer (&window, gtk_window_destroy);
}

static void
test_window_navigation (TestWindowFixture *fixture,
                        gconstpointer      user_data)
{
  GtkWindow *window;
  ValentDevice *device;

  window = g_object_new (VALENT_TYPE_WINDOW,
                         "device-manager", fixture->manager,
                         NULL);
  g_assert_true (VALENT_IS_WINDOW (window));
  g_assert_nonnull (window);

  gtk_window_present (window);

  while (g_main_context_iteration (NULL, FALSE))
    continue;

  /* Main -> Device -> Main */
  g_action_group_activate_action (G_ACTION_GROUP (window),
                                  "page",
                                  g_variant_new_string ("/test-device"));

  g_action_group_activate_action (G_ACTION_GROUP (window),
                                  "page",
                                  g_variant_new_string ("/main"));

  while (g_main_context_iteration (NULL, FALSE))
    continue;

  /* Main -> Device -> Previous */
  g_action_group_activate_action (G_ACTION_GROUP (window),
                                  "page",
                                  g_variant_new_string ("/test-device"));

  g_action_group_activate_action (G_ACTION_GROUP (window),
                                  "previous",
                                  NULL);

  while (g_main_context_iteration (NULL, FALSE))
    continue;

  /* Main -> Device -> Remove Device */
  g_action_group_activate_action (G_ACTION_GROUP (window),
                                  "page",
                                  g_variant_new_string ("/test-device"));

  device = valent_device_manager_get_device (fixture->manager, "test-device");
  g_object_notify (G_OBJECT (device), "state");

  while (g_main_context_iteration (NULL, FALSE))
    continue;

  /* Refresh */
  g_action_group_activate_action (G_ACTION_GROUP (window),
                                  "refresh",
                                  NULL);

  while (g_main_context_iteration (NULL, FALSE))
    continue;

  g_clear_pointer (&window, gtk_window_destroy);
}

static void
test_window_dialogs (TestWindowFixture *fixture,
                     gconstpointer      user_data)
{
  GtkWindow *window;

  /* Preferences */
  window = g_object_new (VALENT_TYPE_WINDOW,
                         "device-manager", fixture->manager,
                         NULL);
  g_assert_true (VALENT_IS_WINDOW (window));
  g_assert_nonnull (window);

  gtk_window_present (window);

  while (g_main_context_iteration (NULL, FALSE))
    continue;

  g_action_group_activate_action (G_ACTION_GROUP (window), "preferences", NULL);

  while (g_main_context_iteration (NULL, FALSE))
    continue;

  g_clear_pointer (&window, gtk_window_destroy);

  /* About */
  window = g_object_new (VALENT_TYPE_WINDOW,
                         "device-manager", fixture->manager,
                         NULL);
  g_assert_true (VALENT_IS_WINDOW (window));
  g_assert_nonnull (window);

  gtk_window_present (window);

  while (g_main_context_iteration (NULL, FALSE))
    continue;

  g_action_group_activate_action (G_ACTION_GROUP (window), "about", NULL);

  while (g_main_context_iteration (NULL, FALSE))
    continue;

  g_clear_pointer (&window, gtk_window_destroy);
}

int
main (int argc,
     char *argv[])
{
  valent_test_ui_init (&argc, &argv, NULL);

  g_test_add ("/libvalent/ui/window/basic",
              TestWindowFixture, NULL,
              test_window_set_up,
              test_window_basic,
              test_window_tear_down);

  g_test_add ("/libvalent/ui/window/navigation",
              TestWindowFixture, NULL,
              test_window_set_up,
              test_window_navigation,
              test_window_tear_down);

  g_test_add ("/libvalent/ui/window/dialogs",
              TestWindowFixture, NULL,
              test_window_set_up,
              test_window_dialogs,
              test_window_tear_down);

  return g_test_run ();
}

