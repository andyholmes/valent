// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gdk/gdk.h>
#include <valent.h>
#include <libvalent-test.h>

#include "test-sms-common.h"
#include "valent-message.h"
#include "valent-message-row.h"


static void
test_sms_message_row (void)
{
  GtkWidget *window, *list;
  GtkWidget *row;

  EContact *contact = NULL;
  g_autoptr (EContact) contact_out = NULL;

  g_autoptr (ValentMessage) message = NULL;
  g_autoptr (ValentMessage) message_out = NULL;
  ValentMessageBox box = VALENT_MESSAGE_BOX_OUTBOX;
  int64_t date = 123456789;
  int64_t id = 987654321;
  GVariant *metadata = g_variant_new_parsed ("{'event': <1>}");
  gboolean read = TRUE;
  const char *sender = "1-234-567-8910";
  const char *text = "Test Message";
  int64_t thread_id = 987321654;
  int64_t thread_id_out;

  contact = valent_test_contact1 ();

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

  VALENT_TEST_CHECK ("Widget can be constructed");
  row = valent_message_row_new (message, contact);

  VALENT_TEST_CHECK ("GObject properties function correctly");
  contact_out = valent_message_row_get_contact (VALENT_MESSAGE_ROW (row));
  message_out = valent_message_row_get_message (VALENT_MESSAGE_ROW (row));
  thread_id_out = valent_message_row_get_thread_id (VALENT_MESSAGE_ROW (row));

  g_assert_true (contact == contact_out);
  g_assert_true (message == message_out);
  g_assert_cmpint (thread_id, ==, thread_id_out);

  g_object_get (row,
                "contact",   &contact_out,
                "message",   &message_out,
                "thread-id", &thread_id_out,
                NULL);

  g_assert_true (contact == contact_out);
  g_assert_true (message == message_out);
  g_assert_cmpint (thread_id, ==, thread_id_out);

  VALENT_TEST_CHECK ("Widget can be realized");
  list = gtk_list_box_new ();
  gtk_list_box_append (GTK_LIST_BOX (list), row);

  window = gtk_window_new ();
  gtk_window_set_child (GTK_WINDOW (window), list);

  gtk_window_present (GTK_WINDOW (window));
  gtk_window_destroy (GTK_WINDOW (window));
}

int
main (int   argc,
      char *argv[])
{
  valent_test_ui_init (&argc, &argv, NULL);

  g_test_add_func ("/plugins/sms/message-row",
                   test_sms_message_row);

  return g_test_run ();
}

