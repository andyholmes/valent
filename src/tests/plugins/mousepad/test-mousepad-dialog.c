// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gdk/gdk.h>
#include <libvalent-core.h>
#include <libvalent-test.h>
#include <libvalent-ui.h>

#include "valent-mousepad-dialog.h"


static void
test_mousepad_dialog (ValentTestFixture *fixture,
                      gconstpointer      user_data)
{
  ValentMousepadDialog *dialog;
  ValentDevice *device;

  dialog = valent_mousepad_dialog_new (fixture->device);
  g_object_ref_sink (dialog);

  /* Properties */
  g_object_get (dialog,
                "device", &device,
                NULL);
  g_assert_true (fixture->device == device);
  g_object_unref (device);

  /* Exercise a few methods */
  valent_mousepad_dialog_echo_key (dialog, "a", 0);
  valent_mousepad_dialog_echo_key (dialog, "a", GDK_CONTROL_MASK);

  valent_mousepad_dialog_echo_special (dialog, GDK_KEY_Linefeed, 0);
  valent_mousepad_dialog_echo_special (dialog, GDK_KEY_BackSpace, 0);
  valent_mousepad_dialog_echo_special (dialog, GDK_KEY_Home, 0);
  valent_mousepad_dialog_echo_special (dialog, GDK_KEY_End, 0);
  valent_mousepad_dialog_echo_special (dialog, GDK_KEY_Page_Up, 0);
  valent_mousepad_dialog_echo_special (dialog, GDK_KEY_Page_Down, 0);
  valent_mousepad_dialog_echo_special (dialog, GDK_KEY_Up, 0);
  valent_mousepad_dialog_echo_special (dialog, GDK_KEY_Down, 0);
  valent_mousepad_dialog_echo_special (dialog, GDK_KEY_Left, 0);
  valent_mousepad_dialog_echo_special (dialog, GDK_KEY_Right, 0);

  g_object_unref (dialog);
}

int
main (int   argc,
      char *argv[])
{
  const char *path = TEST_DATA_DIR"/plugin-mousepad.json";

  valent_test_ui_init (&argc, &argv, NULL);

  g_test_add ("/plugins/mousepad/dialog",
              ValentTestFixture, path,
              valent_test_fixture_init,
              test_mousepad_dialog,
              valent_test_fixture_clear);

  return g_test_run ();
}

