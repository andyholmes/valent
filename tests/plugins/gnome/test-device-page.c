// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <adwaita.h>
#include <gtk/gtk.h>
#include <valent.h>
#include <libvalent-test.h>

#include "test-gnome-common.h"

#define VALENT_TYPE_TEST_SUBJECT (g_type_from_name ("ValentDevicePage"))


static void
test_device_page_basic (ValentTestFixture *fixture,
                        gconstpointer      user_data)
{
  GtkWindow *window;
  GtkWidget *page;
  ValentDevice *device = NULL;
  PeasEngine *engine;

  page = g_object_new (VALENT_TYPE_TEST_SUBJECT,
                       "device", fixture->device,
                       NULL);
  g_assert_nonnull (page);

  window = g_object_new (ADW_TYPE_WINDOW,
                         "content", page,
                         NULL);
  g_object_add_weak_pointer (G_OBJECT (window), (gpointer)&window);

  gtk_window_present (window);
  valent_test_await_pending ();

  VALENT_TEST_CHECK ("GObject properties function correctly");
  g_object_get (page,
                "device", &device,
                NULL);
  g_assert_true (fixture->device == device);
  g_clear_object (&device);

  /* GActions (activate, since state can't be checked) */
  gtk_widget_activate_action (page, "page.unpair", NULL);
  gtk_widget_activate_action (page, "page.preferences", NULL);
  gtk_widget_activate_action (page, "page.pair", NULL);

  /* Unload the plugin */
  engine = valent_get_plugin_engine ();
  peas_engine_unload_plugin (engine,
                             peas_engine_get_plugin_info (engine, "mock"));

  gtk_window_destroy (window);
  valent_test_await_nullptr (&window);
}

static void
test_device_page_dialogs (ValentTestFixture *fixture,
                          gconstpointer      user_data)
{
  GtkWindow *window;
  GtkWidget *page;

  page = g_object_new (VALENT_TYPE_TEST_SUBJECT,
                       "device", fixture->device,
                       NULL);
  g_assert_nonnull (page);

  window = g_object_new (ADW_TYPE_WINDOW,
                         "content", page,
                         NULL);
  g_object_add_weak_pointer (G_OBJECT (window), (gpointer)&window);

  gtk_window_present (window);
  valent_test_await_pending ();

  /* Preferences can be opened, and closed when the window closes */
  gtk_widget_activate_action (page, "page.preferences", NULL);

  gtk_window_destroy (window);
  valent_test_await_nullptr (&window);
}

int
main (int   argc,
      char *argv[])
{
  const char *path = "plugin-mock.json";

  valent_test_gnome_init (&argc, &argv, NULL);

  g_test_add ("/libvalent/ui/device-page/basic",
              ValentTestFixture, path,
              valent_test_fixture_init,
              test_device_page_basic,
              valent_test_fixture_clear);

  g_test_add ("/libvalent/ui/device-page/dialogs",
              ValentTestFixture, path,
              valent_test_fixture_init,
              test_device_page_dialogs,
              valent_test_fixture_clear);

  return g_test_run ();
}

