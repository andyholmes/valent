// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <gtk/gtk.h>
#include <valent.h>

#include "valent-message.h"

G_BEGIN_DECLS

#define VALENT_TYPE_MESSAGE_ROW (valent_message_row_get_type())

G_DECLARE_FINAL_TYPE (ValentMessageRow, valent_message_row, VALENT, MESSAGE_ROW, GtkListBoxRow)

GtkWidget     * valent_message_row_new           (ValentMessage    *message,
                                                  EContact         *contact);
EContact      * valent_message_row_get_contact   (ValentMessageRow *row);
void            valent_message_row_set_contact   (ValentMessageRow *row,
                                                  EContact         *contact);
ValentMessage * valent_message_row_get_message   (ValentMessageRow *row);
void            valent_message_row_set_message   (ValentMessageRow *row,
                                                  ValentMessage    *message);
int64_t         valent_message_row_get_thread_id (ValentMessageRow *row);
void            valent_message_row_update        (ValentMessageRow *row);

G_END_DECLS
