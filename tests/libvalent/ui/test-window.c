// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <valent.h>
#include <libvalent-test.h>

#include "valent-window.h"


typedef struct
{
  ValentDeviceManager *manager;
} TestWindowFixture;

static void
test_window_set_up (TestWindowFixture *fixture,
                    gconstpointer      user_data)
{
  fixture->manager = valent_device_manager_new_sync (NULL, NULL);
  valent_device_manager_start (fixture->manager);
}

static void
test_window_tear_down (TestWindowFixture *fixture,
                       gconstpointer      user_data)
{
  valent_device_manager_stop (fixture->manager);
  v_assert_finalize_object (fixture->manager);
}

static void
test_window_basic (TestWindowFixture *fixture,
                   gconstpointer      user_data)
{
  g_autoptr (ValentDeviceManager) manager = NULL;
  GtkWindow *window;

  window = g_object_new (VALENT_TYPE_WINDOW,
                         "device-manager", fixture->manager,
                         NULL);
  g_object_add_weak_pointer (G_OBJECT (window), (gpointer)&window);

  gtk_window_present (window);
  valent_test_await_pending ();

  /* Properties */
  g_object_get (window,
                "device-manager", &manager,
                NULL);
  g_assert_true (fixture->manager == manager);
  g_clear_object (&manager);

  gtk_window_destroy (window);

  while (window != NULL)
    g_main_context_iteration (NULL, FALSE);
}

VALENT_NO_ASAN static void
test_window_navigation (TestWindowFixture *fixture,
                        gconstpointer      user_data)
{
  GtkWindow *window;
  g_autoptr (ValentDevice) device = NULL;

  window = g_object_new (VALENT_TYPE_WINDOW,
                         "device-manager", fixture->manager,
                         NULL);
  g_object_add_weak_pointer (G_OBJECT (window), (gpointer)&window);

  /* Wait for the window to be presented, then wait for the mock device */
  gtk_window_present (window);
  valent_test_await_pending ();

  gtk_widget_activate_action (GTK_WIDGET (window), "win.refresh", NULL);
  valent_test_await_pending ();

  /* Main -> Device -> Main */
  gtk_widget_activate_action (GTK_WIDGET (window), "win.page", "s", "mock-device");
  gtk_widget_activate_action (GTK_WIDGET (window), "win.previous", NULL);

  /* Main -> Device -> Remove Device */
  gtk_widget_activate_action (GTK_WIDGET (window), "win.page", "s", "mock-device");

  device = g_list_model_get_item (G_LIST_MODEL (fixture->manager), 0);
  valent_device_set_channel (device, NULL);
  valent_test_await_pending ();

  gtk_window_destroy (window);

  while (window != NULL)
    g_main_context_iteration (NULL, FALSE);
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

  gtk_window_present (window);
  valent_test_await_pending ();

  /* Changing the page closes the preferences */
  gtk_widget_activate_action (GTK_WIDGET (window), "win.preferences", NULL);

  gtk_widget_activate_action (GTK_WIDGET (window), "win.page", "s", "main");

  /* Closing the window closed the preferences */
  gtk_widget_activate_action (GTK_WIDGET (window), "win.preferences", NULL);
  valent_test_await_pending ();

  g_clear_pointer (&window, gtk_window_destroy);

#if 0
  // FIXME: throws warning for uninstalled icon

  /* About */
  window = g_object_new (VALENT_TYPE_WINDOW,
                         "device-manager", fixture->manager,
                         NULL);
  g_object_add_weak_pointer (G_OBJECT (window), (gpointer)&window);

  gtk_window_present (window);
  valent_test_await_pending ();

  gtk_widget_activate_action (GTK_WIDGET (window), "win.about", NULL);
  valent_test_await_pending ();

  gtk_window_destroy (window);

  while (window != NULL)
    g_main_context_iteration (NULL, FALSE);
#endif
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

