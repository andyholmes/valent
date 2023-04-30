// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <locale.h>

#include <gio/gio.h>
#include <gtk/gtk.h>
#include <valent.h>
#include <libvalent-test.h>

#define BUS_NAME "org.gnome.Shell"
#define MAIN_PATH "/org/gnome/Mutter/RemoteDesktop"
#define MAIN_IFACE "org.gnome.Mutter.RemoteDesktop"
#define SESSION_IFACE "org.gnome.Mutter.RemoteDesktop.Session"


typedef struct
{
  ValentInput     *input;
  GDBusConnection *connection;
} MutterInputFixture;

static void
mutter_input_fixture_set_up (MutterInputFixture *fixture,
                             gconstpointer       user_data)
{
  g_autoptr (GSettings) settings = NULL;

  /* Disable the mock plugin */
  settings = valent_test_mock_settings ("clipboard");
  g_settings_set_boolean (settings, "enabled", FALSE);

  fixture->input = valent_input_get_default ();
  fixture->connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
}

static void
mutter_input_fixture_tear_down (MutterInputFixture *fixture,
                                gconstpointer       user_data)
{
  g_clear_object (&fixture->connection);
  v_assert_finalize_object (fixture->input);
}

static void
test_mutter_input_adapter (MutterInputFixture *fixture,
                           gconstpointer       user_data)
{
  /* Wait a bit longer for initialization to finish, then pump the adapter to
   * start a remote desktop session.
   */
  valent_test_await_timeout (250);
  valent_input_pointer_motion (fixture->input, 1.0, 0.0);
  valent_test_await_timeout (50);

  VALENT_TEST_CHECK ("Adapter handles relative pointer motion");
  valent_input_pointer_motion (fixture->input, 1.0, 1.0);
  valent_test_await_pending ();

  VALENT_TEST_CHECK ("Adapter handles pointer axis motion");
  valent_input_pointer_axis (fixture->input, 0.0, 1.0);
  valent_test_await_pending ();

  VALENT_TEST_CHECK ("Adapter handles pointer button press");
  valent_input_pointer_button (fixture->input, VALENT_POINTER_PRIMARY, TRUE);
  valent_test_await_pending ();
  VALENT_TEST_CHECK ("Adapter handles pointer button release");
  valent_input_pointer_button (fixture->input, VALENT_POINTER_PRIMARY, FALSE);
  valent_test_await_pending ();

  VALENT_TEST_CHECK ("Adapter handles keyboard key press");
  valent_input_keyboard_keysym (fixture->input, 'a', TRUE);
  valent_test_await_pending ();
  VALENT_TEST_CHECK ("Adapter handles keyboard key release");
  valent_input_keyboard_keysym (fixture->input, 'a', FALSE);
  valent_test_await_pending ();
}

int
main (int   argc,
      char *argv[])
{
  valent_test_init (&argc, &argv, NULL);

  g_test_add ("/plugins/gnome/mutter-input",
              MutterInputFixture, NULL,
              mutter_input_fixture_set_up,
              test_mutter_input_adapter,
              mutter_input_fixture_tear_down);

  return g_test_run ();
}
