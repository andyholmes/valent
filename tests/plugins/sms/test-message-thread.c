// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gdk/gdk.h>
#include <valent.h>
#include <libvalent-test.h>

#include "test-sms-common.h"
#include "valent-message-thread.h"
#include "valent-sms-store.h"


static void
on_thread_items_changed (GListModel   *model,
                         unsigned int  position,
                         unsigned int  removed,
                         unsigned int  added,
                         GMainLoop    *loop)
{
  g_main_loop_quit (loop);
}

static void
on_message_notify_text (ValentMessage *message,
                        GParamSpec    *pspec,
                        GMainLoop     *loop)
{
  g_main_loop_quit (loop);
}

static void
test_sms_message_thread (void)
{
  g_autoptr (GMainLoop) loop = NULL;
  gulong signal_id;

  g_autoptr (ValentSmsStore) store = NULL;
  g_autoptr (GListModel) thread = NULL;
  g_autoptr (ValentMessage) message = NULL;
  g_autoptr (ValentSmsStore) store_out = NULL;
  gint64 id_out;

  loop = g_main_loop_new (NULL, FALSE);
  store = valent_test_sms_store_new ();

  /* Get thread and wait for it to load */
  thread = valent_sms_store_get_thread (store, 1);
  signal_id = g_signal_connect (thread,
                                "items-changed",
                                G_CALLBACK (on_thread_items_changed),
                                loop);
  g_main_loop_run (loop);
  g_clear_signal_handler (&signal_id, thread);

  /* Properties */
  id_out = valent_message_thread_get_id (VALENT_MESSAGE_THREAD (thread));
  store_out = valent_message_thread_get_store (VALENT_MESSAGE_THREAD (thread));

  g_assert_cmpint (id_out, ==, 1);
  g_assert_true (store_out == store);

  g_object_get (thread,
                "id",    &id_out,
                "store", &store_out,
                NULL);

  g_assert_cmpint (id_out, ==, 1);
  g_assert_true (store_out == store);

  /* Get the first item and wait for it to load */
  message = g_list_model_get_item (thread, 0);
  g_signal_connect (message,
                    "notify::text",
                    G_CALLBACK (on_message_notify_text),
                    loop);
  g_main_loop_run (loop);
  g_clear_signal_handler (&signal_id, message);

  g_assert_true (VALENT_IS_MESSAGE (message));
  g_assert_cmpint (valent_message_get_id (message), ==, 1);
  g_assert_cmpstr (valent_message_get_text (message), ==, "Thread 1, Message 1");

  /* Check the item type */
  g_assert_true (g_list_model_get_item_type (thread) == VALENT_TYPE_MESSAGE);

  /* Check the item count */
  g_assert_cmpuint (g_list_model_get_n_items (thread), ==, 2);

  /* Remove a message and wait for GListModel::items-changed */
  /* valent_sms_store_remove_message (store, 1, NULL, NULL, NULL); */
  /* signal_id = g_signal_connect (thread, */
  /*                               "items-changed", */
  /*                               G_CALLBACK (on_thread_items_changed), */
  /*                               loop); */
  /* g_main_loop_run (loop); */
  /* g_clear_signal_handler (&signal_id, thread); */
}

int
main (int   argc,
      char *argv[])
{
  valent_test_init (&argc, &argv, NULL);

  g_test_add_func ("/plugins/sms/message-thread",
                   test_sms_message_thread);

  return g_test_run ();
}

