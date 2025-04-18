// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gtk/gtk.h>
#include <valent.h>
#include <libvalent-test.h>

#include "test-gnome-common.h"

#define VALENT_TYPE_TEST_SUBJECT (g_type_from_name ("ValentWindow"))

typedef struct
{
  ValentDeviceManager *manager;
} TestWindowFixture;

static void
test_window_set_up (TestWindowFixture *fixture,
                    gconstpointer      user_data)
{
  fixture->manager = valent_device_manager_get_default ();
  valent_application_plugin_startup (VALENT_APPLICATION_PLUGIN (fixture->manager));
}

static void
test_window_tear_down (TestWindowFixture *fixture,
                       gconstpointer      user_data)
{
  valent_application_plugin_shutdown (VALENT_APPLICATION_PLUGIN (fixture->manager));
  v_await_finalize_object (fixture->manager);
}

static void
test_window_basic (TestWindowFixture *fixture,
                   gconstpointer      user_data)
{
  g_autoptr (ValentDeviceManager) manager = NULL;
  GtkWindow *window;

  VALENT_TEST_CHECK ("Window can be constructed");
  window = g_object_new (VALENT_TYPE_TEST_SUBJECT,
                         "device-manager", fixture->manager,
                         NULL);
  g_object_add_weak_pointer (G_OBJECT (window), (gpointer)&window);

  VALENT_TEST_CHECK ("Window can be presented");
  gtk_window_present (window);
  valent_test_await_pending ();

  VALENT_TEST_CHECK ("GObject properties function correctly");
  g_object_get (window,
                "device-manager", &manager,
                NULL);
  g_assert_true (fixture->manager == manager);
  g_clear_object (&manager);

  VALENT_TEST_CHECK ("Window can be destroyed");
  gtk_window_destroy (window);
  valent_test_await_nullptr (&window);
}

static void
test_window_device_management (TestWindowFixture *fixture,
                               gconstpointer      user_data)
{
  g_autoptr (ValentDevice) device = NULL;
  GtkWindow *window;

  VALENT_TEST_CHECK ("Window can be constructed");
  window = g_object_new (VALENT_TYPE_TEST_SUBJECT,
                         "device-manager", fixture->manager,
                         NULL);
  g_object_add_weak_pointer (G_OBJECT (window), (gpointer)&window);

  VALENT_TEST_CHECK ("Window can be presented");
  gtk_window_present (window);
  valent_test_await_pending ();

  VALENT_TEST_CHECK ("Window updates when a device is added");
  gtk_widget_activate_action (GTK_WIDGET (window), "win.refresh", NULL);
  valent_test_await_pending ();

  VALENT_TEST_CHECK ("Window updates when the device state changes");
  device = g_list_model_get_item (G_LIST_MODEL (fixture->manager), 0);
  valent_device_set_channel (device, NULL);
  valent_test_await_pending ();

  VALENT_TEST_CHECK ("Window can be destroyed with an active device");
  gtk_window_destroy (window);
  valent_test_await_nullptr (&window);
}

static void
test_window_navigation (TestWindowFixture *fixture,
                        gconstpointer      user_data)
{
  g_test_skip ("FIXME: segmentation fault, probably latent reference");

#if 0
  GtkWindow *window;
  g_autoptr (ValentDevice) device = NULL;

  VALENT_TEST_CHECK ("Window can be constructed");
  window = g_object_new (VALENT_TYPE_TEST_SUBJECT,
                         "device-manager", fixture->manager,
                         NULL);
  g_object_add_weak_pointer (G_OBJECT (window), (gpointer)&window);

  VALENT_TEST_CHECK ("Window can be presented");
  gtk_window_present (window);
  valent_test_await_pending ();

  VALENT_TEST_CHECK ("Window updates when a device is added");
  gtk_widget_activate_action (GTK_WIDGET (window), "win.refresh", NULL);
  valent_test_await_pending ();

  VALENT_TEST_CHECK ("Window can open a device page");
  gtk_widget_activate_action (GTK_WIDGET (window), "win.page", "s", "mock-device");
  valent_test_await_pending ();

  VALENT_TEST_CHECK ("Window can return to the main page");
  gtk_widget_activate_action (GTK_WIDGET (window), "win.page", "s", "main");
  valent_test_await_pending ();

  VALENT_TEST_CHECK ("Window can return to the device page");
  gtk_widget_activate_action (GTK_WIDGET (window), "win.page", "s", "mock-device");
  valent_test_await_pending ();

  VALENT_TEST_CHECK ("Window updates when a device is removed");
  device = g_list_model_get_item (G_LIST_MODEL (fixture->manager), 0);
  valent_device_set_channel (device, NULL);
  valent_test_await_pending ();

  VALENT_TEST_CHECK ("Window can be destroyed without an active device");
  gtk_window_destroy (window);
  valent_test_await_nullptr (&window);
#endif
}

static void
test_window_dialogs (TestWindowFixture *fixture,
                     gconstpointer      user_data)
{
  g_test_skip ("FIXME: segmentation fault, probably latent reference");

#if 0
  GtkWindow *window;

  /* Preferences */
  VALENT_TEST_CHECK ("Window can be constructed");
  window = g_object_new (VALENT_TYPE_TEST_SUBJECT,
                         "device-manager", fixture->manager,
                         NULL);
  g_assert_nonnull (window);

  gtk_window_present (window);
  valent_test_await_pending ();

  /* Changing the page closes the preferences */
  gtk_widget_activate_action (GTK_WIDGET (window), "win.preferences", NULL);
  valent_test_await_pending ();

  gtk_widget_activate_action (GTK_WIDGET (window), "win.page", "s", "main");
  valent_test_await_pending ();

  /* Closing the window closed the preferences */
  /* gtk_widget_activate_action (GTK_WIDGET (window), "win.preferences", NULL); */
  /* valent_test_await_pending (); */

  g_clear_pointer (&window, gtk_window_destroy);

  // FIXME: throws warning for uninstalled icon
  /* About */
  window = g_object_new (VALENT_TYPE_TEST_SUBJECT,
                         "device-manager", fixture->manager,
                         NULL);
  g_object_add_weak_pointer (G_OBJECT (window), (gpointer)&window);

  gtk_window_present (window);
  valent_test_await_pending ();

  gtk_widget_activate_action (GTK_WIDGET (window), "win.about", NULL);
  valent_test_await_pending ();

  gtk_window_destroy (window);
  valent_test_await_nullptr (&window);
#endif
}

int
main (int argc,
     char *argv[])
{
  valent_test_gnome_init (&argc, &argv, NULL);

  g_test_add ("/libvalent/ui/window/basic",
              TestWindowFixture, NULL,
              test_window_set_up,
              test_window_basic,
              test_window_tear_down);

  g_test_add ("/libvalent/ui/window/device-management",
              TestWindowFixture, NULL,
              test_window_set_up,
              test_window_device_management,
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

