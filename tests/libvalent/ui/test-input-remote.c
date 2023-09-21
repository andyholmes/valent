// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <valent.h>
#include <libvalent-test.h>

#include "valent-mock-input-adapter.h"

#define VALENT_TYPE_TEST_SUBJECT (g_type_from_name ("ValentInputRemote"))


static void
test_input_remote (void)
{
  GtkWindow *remote = NULL;
  GListStore *list = NULL;
  g_autoptr (ValentInputAdapter) adapter = NULL;
  g_autoptr (GListStore) adapters = NULL;

  list = g_list_store_new (VALENT_TYPE_INPUT_ADAPTER);
  remote = g_object_new (VALENT_TYPE_TEST_SUBJECT,
                         "adapters", list,
                         NULL);

  VALENT_TEST_CHECK ("GObject properties function correctly");
  g_object_get (remote,
                "adapters", &adapters,
                NULL);
  g_assert_true (list == adapters);
  g_clear_object (&adapters);

  /* Show the window */
  gtk_window_present (remote);
  valent_test_await_pending ();

  /* Add an adapter */
  adapter = g_object_new (VALENT_TYPE_MOCK_INPUT_ADAPTER, NULL);
  g_list_store_append (list, adapter);

  /* Remove the adapter */
  g_list_store_remove (list, 0);

  /* Destroy the window */
  gtk_window_destroy (remote);
  valent_test_await_pending ();
}

int
main (int   argc,
      char *argv[])
{
  valent_test_ui_init (&argc, &argv, NULL);

  g_test_add_func ("/libvalent/ui/input-remote",
                   test_input_remote);

  return g_test_run ();
}

