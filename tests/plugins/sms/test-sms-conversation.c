// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gdk/gdk.h>
#include <valent.h>
#include <libvalent-test.h>

#include "test-sms-common.h"
#include "valent-conversation-page.h"


static void
test_conversation_page (void)
{
  GtkWidget *conversation;
  GtkWidget *window;
  g_autoptr (GMainLoop) loop = NULL;
  g_autoptr (ValentContactStore) contacts = NULL;
  g_autoptr (ValentMessagesAdapter) messages = NULL;
  g_autoptr (ValentContactStore) contacts_out = NULL;
  g_autoptr (ValentMessagesAdapter) messages_out = NULL;

  loop = g_main_loop_new (NULL, FALSE);
  contacts = valent_test_contact_store_new ();
  messages = valent_test_message_store_new ();
  thread_id = 1;

  VALENT_TEST_CHECK ("Widget can be constructed");
  conversation = g_object_new (VALENT_TYPE_CONVERSATION_PAGE,
                               "contact-store", contacts,
                               "messages", messages,
                               "thread-id",     thread_id,
                               NULL);

  VALENT_TEST_CHECK ("Widget can be realized");
  window = g_object_new (GTK_TYPE_WINDOW,
                         "child",          conversation,
                         "default-height", 480,
                         "default-width",  600,
                         NULL);
  g_object_add_weak_pointer (G_OBJECT (window), (gpointer)&window);

  gtk_window_present (GTK_WINDOW (window));
  valent_test_await_pending ();

  VALENT_TEST_CHECK ("GObject properties function correctly");
  g_object_get (conversation,
                "contact-store", &contacts_out,
                "messages", &messages_out,
                NULL);
  g_assert_true (contacts == contacts_out);
  g_assert_true (messages == messages_out);

  gtk_window_destroy (GTK_WINDOW (window));
  valent_test_await_nullptr (&window);
}

int
main (int   argc,
      char *argv[])
{
  valent_test_ui_init (&argc, &argv, NULL);

  g_test_add_func ("/plugins/sms/conversation",
                   test_conversation_page);

  return g_test_run ();
}

