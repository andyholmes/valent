// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <gtk/gtk.h>
#include <libebook-contacts/libebook-contacts.h>
#include <valent.h>

G_BEGIN_DECLS

#define VALENT_TYPE_CONVERSATION_ROW (valent_conversation_row_get_type())

G_DECLARE_FINAL_TYPE (ValentConversationRow, valent_conversation_row, VALENT, CONVERSATION_ROW, GtkListBoxRow)

GtkWidget     * valent_conversation_row_new         (ValentMessage *message,
                                                     EContact      *contact);
int             valent_conversation_row_collate     (ValentConversationRow *row1,
                                                     ValentConversationRow *row2);

EContact      * valent_conversation_row_get_contact (ValentConversationRow *row);
void            valent_conversation_row_set_contact (ValentConversationRow *row,
                                                     EContact              *contact);
int64_t         valent_conversation_row_get_date    (ValentConversationRow *row);
ValentMessage * valent_conversation_row_get_message (ValentConversationRow *row);
void            valent_conversation_row_set_message (ValentConversationRow *row,
                                                     ValentMessage         *message);
gboolean        valent_conversation_row_is_incoming (ValentConversationRow *row);
void            valent_conversation_row_show_avatar (ValentConversationRow *row,
                                                     gboolean               visible);

G_END_DECLS

