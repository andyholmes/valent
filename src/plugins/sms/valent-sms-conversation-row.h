// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <gtk/gtk.h>
#include <libvalent-contacts.h>

#include "valent-sms-message.h"

G_BEGIN_DECLS

#define VALENT_TYPE_SMS_CONVERSATION_ROW (valent_sms_conversation_row_get_type())

G_DECLARE_FINAL_TYPE (ValentSmsConversationRow, valent_sms_conversation_row, VALENT, SMS_CONVERSATION_ROW, GtkListBoxRow)

GtkWidget        * valent_sms_conversation_row_new           (ValentSmsMessage         *message,
                                                              EContact                 *contact);
int                valent_sms_conversation_row_collate       (ValentSmsConversationRow *row1,
                                                              ValentSmsConversationRow *row2);

gint64             valent_sms_conversation_row_get_thread_id (ValentSmsConversationRow *row);
EContact         * valent_sms_conversation_row_get_contact   (ValentSmsConversationRow *row);
void               valent_sms_conversation_row_set_contact   (ValentSmsConversationRow *row,
                                                              EContact                 *contact);
gint64             valent_sms_conversation_row_get_date      (ValentSmsConversationRow *row);
ValentSmsMessage * valent_sms_conversation_row_get_message   (ValentSmsConversationRow *row);
void               valent_sms_conversation_row_set_message   (ValentSmsConversationRow *row,
                                                              ValentSmsMessage         *message);
gboolean           valent_sms_conversation_row_is_incoming   (ValentSmsConversationRow *row);
void               valent_sms_conversation_row_update        (ValentSmsConversationRow *row);
void               valent_sms_conversation_row_show_avatar   (ValentSmsConversationRow *row,
                                                              gboolean                  visible);

G_END_DECLS

