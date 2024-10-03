// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gdk/gdk.h>
#include <valent.h>
#include <libvalent-test.h>

#include "valent-messages-window.h"


static void
test_messages_window (void)
{
  ValentMessagesWindow *window;
  g_autoptr (ValentMessages) messages_out = NULL;

  /* Prepare Stores */
  VALENT_TEST_CHECK ("Window can be constructed");
  window = g_object_new (VALENT_TYPE_MESSAGES_WINDOW,
                         NULL);
  g_object_add_weak_pointer (G_OBJECT (window), (gpointer)&window);

  gtk_window_present (GTK_WINDOW (window));
  valent_test_await_pending ();

  VALENT_TEST_CHECK ("GObject properties function correctly");
  g_object_get (window,
                "messages", &messages_out,
                NULL);
  g_assert_true (valent_messages_get_default () == messages_out);

  VALENT_TEST_CHECK ("Window action `win.new` starts a conversation");
  g_action_group_activate_action (G_ACTION_GROUP (window),
                                  "new",
                                  NULL);
  valent_test_await_pending ();

  VALENT_TEST_CHECK ("Window action `win.previous` closes a conversation");
  g_action_group_activate_action (G_ACTION_GROUP (window),
                                  "previous",
                                  NULL);
  valent_test_await_pending ();

  VALENT_TEST_CHECK ("Window action `win.search` opens the search page");
  g_action_group_activate_action (G_ACTION_GROUP (window),
                                  "search",
                                  NULL);
  valent_test_await_pending ();

  gtk_window_destroy (GTK_WINDOW (window));
  valent_test_await_nullptr (&window);
}

int
main (int   argc,
      char *argv[])
{
  valent_test_ui_init (&argc, &argv, NULL);

  g_test_add_func ("/plugins/gnome/messages-window",
                   test_messages_window);

  return g_test_run ();
}

