// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gdk/gdk.h>
#include <valent.h>
#include <libvalent-test.h>

#include "valent-message.h"
#include "valent-sms-conversation-row.h"


static void
test_sms_conversation_row (void)
{
  GtkWidget *window, *list;
  GtkWidget *row;

  g_autoptr (EContact) contact = NULL;
  g_autoptr (EContact) contact_out = NULL;
  g_autoptr (GBytes) bytes = NULL;

  g_autoptr (ValentMessage) message = NULL;
  g_autoptr (ValentMessage) message_out = NULL;
  ValentMessageBox box = VALENT_MESSAGE_BOX_OUTBOX;
  int64_t date = 123456789;
  int64_t date_out;
  int64_t id = 987654321;
  GVariant *metadata = g_variant_new_parsed ("{'event': <1>}");
  gboolean read = TRUE;
  const char *sender = "1-234-567-8910";
  const char *text = "Test Message https://www.gnome.org";
  int64_t thread_id = 987321654;

  bytes = g_resources_lookup_data ("/tests/contact.vcf", 0, NULL);
  contact = e_contact_new_from_vcard (g_bytes_get_data (bytes, NULL));

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

  /* Construction */
  row = valent_sms_conversation_row_new (message, contact);

  /* Properties */
  contact_out = valent_sms_conversation_row_get_contact (VALENT_SMS_CONVERSATION_ROW (row));
  message_out = valent_sms_conversation_row_get_message (VALENT_SMS_CONVERSATION_ROW (row));
  date_out = valent_sms_conversation_row_get_date (VALENT_SMS_CONVERSATION_ROW (row));

  g_assert_true (contact == contact_out);
  g_assert_true (message == message_out);
  g_assert_cmpint (date, ==, date_out);
  g_assert_cmpint (id, ==, valent_sms_conversation_row_get_id (VALENT_SMS_CONVERSATION_ROW (row)));
  g_assert_false (valent_sms_conversation_row_is_incoming (VALENT_SMS_CONVERSATION_ROW (row)));

  g_object_get (row,
                "contact", &contact_out,
                "message", &message_out,
                "date",    &date_out,
                NULL);

  g_assert_true (contact == contact_out);
  g_assert_true (message == message_out);
  g_assert_cmpint (date, ==, date_out);

  /* Display */
  list = gtk_list_box_new ();
  gtk_list_box_append (GTK_LIST_BOX (list), row);

  window = gtk_window_new ();
  gtk_window_set_child (GTK_WINDOW (window), list);

  gtk_window_present (GTK_WINDOW (window));

  valent_sms_conversation_row_show_avatar (VALENT_SMS_CONVERSATION_ROW (row),
                                           TRUE);
  valent_sms_conversation_row_show_avatar (VALENT_SMS_CONVERSATION_ROW (row),
                                           FALSE);

  gtk_window_destroy (GTK_WINDOW (window));
}

int
main (int   argc,
      char *argv[])
{
  valent_test_ui_init (&argc, &argv, NULL);

  g_test_add_func ("/plugins/sms/conversation-row",
                   test_sms_conversation_row);

  return g_test_run ();
}

