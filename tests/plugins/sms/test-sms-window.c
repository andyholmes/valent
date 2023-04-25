// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gdk/gdk.h>
#include <valent.h>
#include <libvalent-test.h>

#include "test-sms-common.h"
#include "valent-sms-window.h"


static void
test_sms_window (void)
{
  g_autoptr (ValentSmsStore) messages = NULL;
  g_autoptr (ValentContactStore) contacts = NULL;
  g_autoptr (ValentSmsStore) messages_out = NULL;
  g_autoptr (ValentContactStore) contacts_out = NULL;
  ValentSmsWindow *window;

  /* Prepare Stores */
  contacts = valent_test_contact_store_new ();
  messages = valent_test_sms_store_new ();

  VALENT_TEST_CHECK ("Window can be constructed");
  window = g_object_new (VALENT_TYPE_SMS_WINDOW,
                         "contact-store", contacts,
                         "message-store", messages,
                         NULL);
  g_object_add_weak_pointer (G_OBJECT (window), (gpointer)&window);

  gtk_window_present (GTK_WINDOW (window));
  valent_test_await_pending ();

  VALENT_TEST_CHECK ("GObject properties function correctly");
  contacts_out = valent_sms_window_get_contact_store (window);
  messages_out = valent_sms_window_get_message_store (window);

  g_assert_true (contacts == contacts_out);
  g_assert_true (messages == messages_out);

  g_object_get (window,
                "contact-store", &contacts_out,
                "message-store", &messages_out,
                NULL);

  g_assert_true (contacts == contacts_out);
  g_assert_true (messages == messages_out);

  VALENT_TEST_CHECK ("Window action `win.new` starts a conversation");
  g_action_group_activate_action (G_ACTION_GROUP (window),
                                  "new",
                                  NULL);
  valent_test_await_pending ();

  VALENT_TEST_CHECK ("Window method `search_contacts()` can search by name");
  valent_sms_window_search_contacts (window, "num");
  valent_test_await_pending ();

  VALENT_TEST_CHECK ("Window method `search_contacts()` can search by number");
  valent_sms_window_search_contacts (window, "123");
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

  VALENT_TEST_CHECK ("Window method `search_messages()` can search by word");
  valent_sms_window_search_messages (window, "Thread");
  valent_test_await_pending ();

  VALENT_TEST_CHECK ("Window method `set_active_thread()` can open a conversation");
  valent_sms_window_set_active_thread (window, 1);
  valent_test_await_pending ();

  gtk_window_destroy (GTK_WINDOW (window));
  valent_test_await_nullptr (&window);
}

int
main (int   argc,
      char *argv[])
{
  valent_test_ui_init (&argc, &argv, NULL);

  g_test_add_func ("/plugins/sms/window",
                   test_sms_window);

  return g_test_run ();
}

