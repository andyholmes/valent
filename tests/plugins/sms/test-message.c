// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <valent.h>
#include <libvalent-test.h>

#include "valent-message.h"


static void
test_sms_message (void)
{
  g_autoptr (ValentMessage) message = NULL;

  ValentMessageBox box = VALENT_MESSAGE_BOX_OUTBOX;
  int64_t date = 123456789;
  int64_t id = 987654321;
  GVariant *metadata = g_variant_new_parsed ("{'event': <1>}");
  gboolean read = TRUE;
  const char *sender = "1-234-567-8910";
  const char *text = "Test Message";
  int64_t thread_id = 987321654;

  ValentMessageBox box2;
  int64_t date2;
  int64_t id2;
  g_autoptr (GVariant) metadata2 = NULL;
  gboolean read2;
  g_autofree char *sender2 = NULL;
  g_autofree char *text2 = NULL;
  int64_t thread_id2;

  VALENT_TEST_CHECK ("Object can be constructed");
  message = g_object_new (VALENT_TYPE_MESSAGE,
                          "box",       box,
                          "date",      date,
                          "id",        id,
                          "metadata",  metadata,
                          "read",      read,
                          "sender",    sender,
                          "text",      text,
                          "thread-id", thread_id,
                          NULL);

  VALENT_TEST_CHECK ("GObject properties function correctly");
  g_object_get (message,
                "box",       &box2,
                "date",      &date2,
                "id",        &id2,
                "metadata",  &metadata2,
                "read",      &read2,
                "sender",    &sender2,
                "text",      &text2,
                "thread-id", &thread_id2,
                NULL);

  g_assert_cmpuint (box, ==, box2);
  g_assert_cmpint (date, ==, date2);
  g_assert_cmpint (id, ==, id2);
  g_assert_true (g_variant_equal (metadata, metadata2));
  g_assert_true (read == read2);
  g_assert_cmpstr (sender, ==, sender2);
  g_assert_cmpstr (text, ==, text2);
  g_assert_cmpint (thread_id, ==, thread_id2);
}

int
main (int   argc,
      char *argv[])
{
  valent_test_init (&argc, &argv, NULL);

  g_test_add_func ("/plugins/sms/message",
                   test_sms_message);

  return g_test_run ();
}

