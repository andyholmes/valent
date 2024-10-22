// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <valent.h>
#include <libvalent-test.h>

static void
test_message_basic (void)
{
  g_autoptr (ValentMessage) message = NULL;

  g_autoptr (GListStore) attachments = g_list_store_new (VALENT_TYPE_MESSAGE_ATTACHMENT);
  ValentMessageBox box = VALENT_MESSAGE_BOX_OUTBOX;
  int64_t date = 123456789;
  int64_t id = 987654321;
  gboolean read = TRUE;
  GStrv recipients = (char *[]){ "1-234-567-8911", NULL, };
  const char *sender = "1-234-567-8910";
  int64_t subscription_id = 2;
  const char *text = "Test Message";
  int64_t thread_id = 987321654;

  g_autoptr (GListStore) attachments2 = NULL;
  ValentMessageBox box2;
  int64_t date2;
  int64_t id2;
  g_autoptr (GVariant) metadata2 = NULL;
  gboolean read2;
  g_auto (GStrv) recipients2 = NULL;
  g_autofree char *sender2 = NULL;
  int64_t subscription_id2 = 2;
  g_autofree char *text2 = NULL;
  int64_t thread_id2;

  VALENT_TEST_CHECK ("Object can be constructed");
  message = g_object_new (VALENT_TYPE_MESSAGE,
                          "attachments",     attachments,
                          "box",             box,
                          "date",            date,
                          "id",              id,
                          "read",            read,
                          "recipients",      recipients,
                          "sender",          sender,
                          "subscription-id", subscription_id,
                          "text",            text,
                          "thread-id",       thread_id,
                          NULL);

  VALENT_TEST_CHECK ("GObject properties function correctly");
  g_object_get (message,
                "attachments",     &attachments2,
                "box",             &box2,
                "date",            &date2,
                "id",              &id2,
                "read",            &read2,
                "recipients",      &recipients2,
                "sender",          &sender2,
                "subscription-id", &subscription_id2,
                "text",            &text2,
                "thread-id",       &thread_id2,
                NULL);

  g_assert_true (attachments == attachments2);
  g_assert_cmpuint (box, ==, box2);
  g_assert_cmpint (date, ==, date2);
  g_assert_cmpint (id, ==, id2);
  g_assert_true (read == read2);
  g_assert_false (recipients == recipients2); // TODO: strv compare
  g_assert_cmpstr (sender, ==, sender2);
  g_assert_cmpint (subscription_id, ==, subscription_id2);
  g_assert_cmpstr (text, ==, text2);
  g_assert_cmpint (thread_id, ==, thread_id2);

  VALENT_TEST_CHECK ("Property getters function correctly");
  g_assert_true (G_LIST_MODEL (attachments) == valent_message_get_attachments (message));
  g_assert_cmpuint (box, ==, valent_message_get_box (message));
  g_assert_cmpint (date, ==, valent_message_get_date (message));
  g_assert_cmpint (id, ==, valent_message_get_id (message));
  g_assert_true (read == valent_message_get_read (message));
  g_assert_false ((const char * const *)recipients == valent_message_get_recipients (message)); // TODO: strv compare
  g_assert_cmpstr (sender, ==, valent_message_get_sender (message));
  g_assert_cmpint (subscription_id, ==, valent_message_get_subscription_id (message));
  g_assert_cmpstr (text, ==, valent_message_get_text (message));
  g_assert_cmpint (thread_id, ==, valent_message_get_thread_id (message));
}

int
main (int   argc,
      char *argv[])
{
  valent_test_init (&argc, &argv, NULL);

  g_test_add_func ("/libvalent/messages/message",
                   test_message_basic);

  return g_test_run ();
}

