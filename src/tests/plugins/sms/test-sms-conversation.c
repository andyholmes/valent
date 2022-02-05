// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gdk/gdk.h>
#include <libvalent-core.h>
#include <libvalent-test.h>
#include <libvalent-ui.h>

#include "test-sms-common.h"
#include "valent-sms-conversation.h"


static gboolean
timeout_func (GMainLoop *loop)
{
  g_main_loop_quit (loop);

  return G_SOURCE_REMOVE;
}


static void
test_sms_conversation (void)
{
  GtkWidget *conversation;
  GtkWidget *window;
  g_autoptr (GMainLoop) loop = NULL;
  g_autoptr (ValentContactStore) contacts = NULL;
  g_autoptr (ValentSmsStore) messages = NULL;
  g_autoptr (ValentContactStore) contacts_out = NULL;
  g_autoptr (ValentSmsStore) messages_out = NULL;
  gint64 thread_id, thread_id_out;

  loop = g_main_loop_new (NULL, FALSE);
  contacts = valent_test_contact_store_new ();
  messages = valent_test_sms_store_new ();
  thread_id = 1;

  /* Construction */
  conversation = g_object_new (VALENT_TYPE_SMS_CONVERSATION,
                               "contact-store", contacts,
                               "message-store", messages,
                               "thread-id",     thread_id,
                               NULL);

  /* Display */
  window = g_object_new (GTK_TYPE_WINDOW,
                         "child",          conversation,
                         "default-height", 480,
                         "default-width",  600,
                         NULL);
  gtk_window_present (GTK_WINDOW (window));

  g_timeout_add_seconds (1, (GSourceFunc)timeout_func, loop);
  g_main_loop_run (loop);

  /* Properties */
  g_object_get (conversation,
                "contact-store", &contacts_out,
                "message-store", &messages_out,
                "thread-id",     &thread_id_out,
                NULL);
  g_assert_true (contacts == contacts_out);
  g_assert_true (messages == messages_out);
  g_assert_cmpint (thread_id, ==, thread_id_out);
  g_assert_cmpint (thread_id, ==, valent_sms_conversation_get_thread_id (VALENT_SMS_CONVERSATION (conversation)));

  gtk_window_destroy (GTK_WINDOW (window));
}

int
main (int   argc,
      char *argv[])
{
  valent_test_ui_init (&argc, &argv, NULL);

  g_test_add_func ("/plugins/sms/conversation",
                   test_sms_conversation);

  return g_test_run ();
}

