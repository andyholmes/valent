// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <valent.h>
#include <libvalent-test.h>

#include "valent-presenter-remote.h"


static void
test_presenter_remote (ValentTestFixture *fixture,
                       gconstpointer      user_data)
{
  GtkWindow *remote = NULL;
  ValentDevice *device = NULL;

  VALENT_TEST_CHECK ("Plugin can be constructed");
  remote = g_object_new (VALENT_TYPE_PRESENTER_REMOTE,
                         "device", fixture->device,
                         NULL);

  VALENT_TEST_CHECK ("GObject properties function correctly");
  g_object_get (remote,
                "device", &device,
                NULL);
  g_assert_true (fixture->device == device);
  g_clear_object (&device);

  gtk_window_present (remote);
  valent_test_await_pending ();

  gtk_window_destroy (remote);
  valent_test_await_pending ();
}

int
main (int   argc,
      char *argv[])
{
  const char *path = "plugin-presenter.json";

  valent_test_ui_init (&argc, &argv, NULL);

  g_test_add ("/plugins/presenter/remote",
              ValentTestFixture, path,
              valent_test_fixture_init,
              test_presenter_remote,
              valent_test_fixture_clear);

  return g_test_run ();
}

