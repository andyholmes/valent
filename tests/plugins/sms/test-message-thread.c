// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gdk/gdk.h>
#include <valent.h>
#include <libvalent-test.h>

#include "test-sms-common.h"
#include "valent-message-thread.h"
#include "valent-sms-store.h"


static void
test_sms_message_thread (void)
{
  g_autoptr (ValentSmsStore) store = NULL;
  g_autoptr (GListModel) thread = NULL;
  g_autoptr (ValentMessage) message = NULL;
  g_autoptr (ValentSmsStore) store_out = NULL;
  int64_t id_out;

  store = valent_test_sms_store_new ();

  VALENT_TEST_CHECK ("Thread can be constructed");
  thread = valent_sms_store_get_thread (store, 1);
  valent_test_await_signal (thread, "items-changed");

  VALENT_TEST_CHECK ("GObject properties function correctly");
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

  VALENT_TEST_CHECK ("Thread loads message asynchronously");
  message = g_list_model_get_item (thread, 0);
  valent_test_await_signal (message, "notify::text");

  g_assert_true (VALENT_IS_MESSAGE (message));
  g_assert_cmpint (valent_message_get_id (message), ==, 1);
  g_assert_cmpstr (valent_message_get_text (message), ==, "Thread 1, Message 1");

  VALENT_TEST_CHECK ("Thread implements `GListModel` correctly");
  g_assert_true (g_list_model_get_item_type (thread) == VALENT_TYPE_MESSAGE);
  g_assert_cmpuint (g_list_model_get_n_items (thread), ==, 2);

  /* Remove a message and wait for GListModel::items-changed */
  /* valent_sms_store_remove_message (store, 1, NULL, NULL, NULL); */
  /* valent_test_await_signal (thread, "items-changed"); */
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

