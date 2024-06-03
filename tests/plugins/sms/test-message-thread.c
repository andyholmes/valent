// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gdk/gdk.h>
#include <valent.h>
#include <libvalent-test.h>

#include "test-sms-common.h"
#include "valent-message-store.h"
#include "valent-message-thread-private.h"


static void
test_sms_message_thread (void)
{
  g_autoptr (ValentMessagesAdapter) store = NULL;
  g_autoptr (GListModel) thread = NULL;
  g_autoptr (ValentMessage) message = NULL;
  g_autoptr (ValentMessagesAdapter) store_out = NULL;
  g_autofree char *iri_out = NULL;

  store = valent_test_message_store_new ();

  VALENT_TEST_CHECK ("Thread can be constructed");
  thread = g_list_model_get_item (G_LIST_MODE (store), 0);
  valent_test_await_signal (store, "items-changed");

  VALENT_TEST_CHECK ("GObject properties function correctly");
  g_object_get (thread,
                "iri",   &iri_out,
                "store", &store_out,
                NULL);

  g_assert_cmpstr (iri_out, ==, "");
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
  /* valent_messages_adapter_remove_message (store, 1, NULL, NULL, NULL); */
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

