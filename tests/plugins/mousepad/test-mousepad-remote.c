// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gdk/gdk.h>
#include <valent.h>
#include <libvalent-test.h>

#include "valent-mousepad-remote.h"


static void
test_mousepad_remote (ValentTestFixture *fixture,
                      gconstpointer      user_data)
{
  GtkWindow *remote;
  ValentDevice *device;

  VALENT_TEST_CHECK ("Remote can be constructed");
  remote = g_object_new (VALENT_TYPE_MOUSEPAD_REMOTE,
                         "device", fixture->device,
                         NULL);

  VALENT_TEST_CHECK ("GObject properties function correctly");
  g_object_get (remote,
                "device", &device,
                NULL);
  g_assert_true (fixture->device == device);
  g_object_unref (device);

  VALENT_TEST_CHECK ("Remote `echo_key()` functions correctly");
  valent_mousepad_remote_echo_key (VALENT_MOUSEPAD_REMOTE (remote), "a", 0);
  valent_mousepad_remote_echo_key (VALENT_MOUSEPAD_REMOTE (remote), "a", GDK_CONTROL_MASK);

  VALENT_TEST_CHECK ("Remote `echo_special()` functions correctly");
  valent_mousepad_remote_echo_special (VALENT_MOUSEPAD_REMOTE (remote), GDK_KEY_Linefeed, 0);
  valent_mousepad_remote_echo_special (VALENT_MOUSEPAD_REMOTE (remote), GDK_KEY_BackSpace, 0);
  valent_mousepad_remote_echo_special (VALENT_MOUSEPAD_REMOTE (remote), GDK_KEY_Home, 0);
  valent_mousepad_remote_echo_special (VALENT_MOUSEPAD_REMOTE (remote), GDK_KEY_End, 0);
  valent_mousepad_remote_echo_special (VALENT_MOUSEPAD_REMOTE (remote), GDK_KEY_Page_Up, 0);
  valent_mousepad_remote_echo_special (VALENT_MOUSEPAD_REMOTE (remote), GDK_KEY_Page_Down, 0);
  valent_mousepad_remote_echo_special (VALENT_MOUSEPAD_REMOTE (remote), GDK_KEY_Up, 0);
  valent_mousepad_remote_echo_special (VALENT_MOUSEPAD_REMOTE (remote), GDK_KEY_Down, 0);
  valent_mousepad_remote_echo_special (VALENT_MOUSEPAD_REMOTE (remote), GDK_KEY_Left, 0);
  valent_mousepad_remote_echo_special (VALENT_MOUSEPAD_REMOTE (remote), GDK_KEY_Right, 0);

  g_clear_pointer (&remote, gtk_window_destroy);
}

int
main (int   argc,
      char *argv[])
{
  const char *path = "plugin-mousepad.json";

  valent_test_ui_init (&argc, &argv, NULL);

  g_test_add ("/plugins/mousepad/dialog",
              ValentTestFixture, path,
              valent_test_fixture_init,
              test_mousepad_remote,
              valent_test_fixture_clear);

  return g_test_run ();
}

