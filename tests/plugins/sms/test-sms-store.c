// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gdk/gdk.h>
#include <valent.h>
#include <libvalent-test.h>

#include "test-sms-common.h"
#include "valent-sms-store.h"


static int n_messages = 0;
static int n_added = 0;
static int n_changed = 0;
static int n_removed = 0;


static void
add_messages_cb (ValentSmsStore *store,
                 GAsyncResult   *result,
                 GMainLoop      *loop)
{
  g_autoptr (GError) error = NULL;

  valent_sms_store_add_messages_finish (store, result, &error);
  g_assert_no_error (error);

  g_main_loop_quit (loop);
}

static void
find_messages_cb (ValentSmsStore *store,
                  GAsyncResult   *result,
                  GMainLoop      *loop)
{
  g_autoptr (GPtrArray) messages = NULL;
  g_autoptr (GError) error = NULL;

  messages = valent_sms_store_find_messages_finish (store, result, &error);
  g_assert_cmpuint (messages->len, ==, 2);
  g_assert_no_error (error);

  g_main_loop_quit (loop);
}

static void
get_message_cb (ValentSmsStore *store,
                GAsyncResult   *result,
                GMainLoop      *loop)
{
  g_autoptr (ValentMessage) message = NULL;
  g_autoptr (GError) error = NULL;

  message = valent_sms_store_get_message_finish (store, result, &error);
  g_assert_no_error (error);
  g_assert_true (VALENT_IS_MESSAGE (message));
  g_assert_cmpint (valent_message_get_id (message), ==, 1);

  g_main_loop_quit (loop);
}

static void
remove_message_cb (ValentSmsStore *store,
                   GAsyncResult   *result,
                   GMainLoop      *loop)
{
  g_autoptr (GError) error = NULL;

  valent_sms_store_remove_message_finish (store, result, &error);
  g_assert_no_error (error);

  g_main_loop_quit (loop);
}

static void
remove_thread_cb (ValentSmsStore *store,
                  GAsyncResult   *result,
                  GMainLoop      *loop)
{
  g_autoptr (GError) error = NULL;

  valent_sms_store_remove_thread_finish (store, result, &error);
  g_assert_no_error (error);

  g_main_loop_quit (loop);
}

static void
on_summary_items_changed (GListModel   *model,
                          unsigned int  position,
                          unsigned int  removed,
                          unsigned int  added,
                          GMainLoop    *loop)
{
  g_main_loop_quit (loop);
}

static void
on_message_added (ValentSmsStore *store,
                  ValentMessage  *message)
{
  n_added++;
  n_messages++;
}

static void
on_message_changed (ValentSmsStore *store,
                    ValentMessage  *message)
{
  n_changed++;
}

static void
on_message_removed (ValentSmsStore *store,
                    ValentMessage  *message)
{
  n_removed++;
  n_messages--;
}


static void
test_sms_store (void)
{
  g_autoptr (GMainLoop) loop = NULL;
  gulong signal_id;

  g_autoptr (ValentContext) context = NULL;
  g_autoptr (ValentSmsStore) store = NULL;
  g_autoptr (GPtrArray) messages = NULL;
  g_autoptr (GListModel) summary = NULL;
  gint64 thread_date;

  loop = g_main_loop_new (NULL, FALSE);

  /* Prepare Store */
  context = g_object_new (VALENT_TYPE_CONTEXT,
                          "domain", "device",
                          "id",     "test-device",
                          NULL);
  store = valent_sms_store_new (context);
  messages = valent_test_sms_get_messages ();

  g_signal_connect (G_OBJECT (store),
                    "message-added",
                    G_CALLBACK (on_message_added),
                    NULL);

  g_signal_connect (G_OBJECT (store),
                    "message-changed",
                    G_CALLBACK (on_message_changed),
                    NULL);

  g_signal_connect (G_OBJECT (store),
                    "message-removed",
                    G_CALLBACK (on_message_removed),
                    NULL);

  /* Add messages */
  valent_sms_store_add_messages (store,
                                 messages,
                                 NULL,
                                 (GAsyncReadyCallback)add_messages_cb,
                                 loop);
  g_main_loop_run (loop);
  g_assert_cmpint (n_added, ==, 3);

  /* Update a message */
  valent_sms_store_add_message (store,
                                g_ptr_array_index (messages, 2),
                                NULL,
                                (GAsyncReadyCallback)add_messages_cb,
                                loop);
  g_main_loop_run (loop);
  g_assert_cmpint (n_changed, ==, 1);

  /* Thread Date */
  thread_date = valent_sms_store_get_thread_date (store, 1);
  g_assert_cmpint (thread_date, ==, 2);

  thread_date = valent_sms_store_get_thread_date (store, 2);
  g_assert_cmpint (thread_date, ==, 3);

  /* Find Messages (expect 2 results) */
  valent_sms_store_find_messages (store,
                                  "Message 1",
                                  NULL,
                                  (GAsyncReadyCallback)find_messages_cb,
                                  loop);
  g_main_loop_run (loop);

  /* Get Message (expect 1 result) */
  valent_sms_store_get_message (store,
                                1,
                                NULL,
                                (GAsyncReadyCallback)get_message_cb,
                                loop);
  g_main_loop_run (loop);

  /* Get thread (expect 2 items) */
  summary = valent_sms_store_get_thread (store, 1);
  signal_id = g_signal_connect (summary,
                                "items-changed",
                                G_CALLBACK (on_summary_items_changed),
                                loop);
  g_main_loop_run (loop);
  g_clear_signal_handler (&signal_id, summary);
  g_clear_object (&summary);

  /* Get summary (expect 2 items) */
  summary = valent_sms_store_get_summary (store);
  signal_id = g_signal_connect (summary,
                                "items-changed",
                                G_CALLBACK (on_summary_items_changed),
                                loop);
  g_main_loop_run (loop);
  g_clear_signal_handler (&signal_id, summary);

  /* Remove a thread (expect 2 signals) */
  valent_sms_store_remove_thread (store,
                                  1,
                                  NULL,
                                  (GAsyncReadyCallback)remove_thread_cb,
                                  loop);
  g_main_loop_run (loop);
  g_assert_cmpint (n_removed, ==, 2);

  /* Remove a message from a thread (expect 1 signal) */
  valent_sms_store_remove_message (store,
                                   3,
                                   NULL,
                                   (GAsyncReadyCallback)remove_message_cb,
                                   loop);
  g_main_loop_run (loop);
  g_assert_cmpint (n_removed, ==, 3);

  /* Store should be empty now */
  g_assert_cmpint (n_messages, ==, 0);
}

int
main (int   argc,
      char *argv[])
{
  valent_test_init (&argc, &argv, NULL);

  g_test_add_func ("/plugins/sms/store",
                   test_sms_store);

  return g_test_run ();
}

