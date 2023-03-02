// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <gtk/gtk.h>
#include <valent.h>

#include "valent-message.h"
#include "valent-sms-store.h"

G_BEGIN_DECLS

#define VALENT_TYPE_SMS_CONVERSATION (valent_sms_conversation_get_type())

G_DECLARE_FINAL_TYPE (ValentSmsConversation, valent_sms_conversation, VALENT, SMS_CONVERSATION, GtkWidget)

GtkWidget  * valent_sms_conversation_new               (ValentContactStore    *contacts,
                                                        ValentSmsStore        *messages);

gint64       valent_sms_conversation_get_thread_id     (ValentSmsConversation *conversation);
void         valent_sms_conversation_set_thread_id     (ValentSmsConversation *conversation,
                                                        gint64                 thread_id);
const char * valent_sms_conversation_get_title         (ValentSmsConversation *conversation);
const char * valent_sms_conversation_get_subtitle      (ValentSmsConversation *conversation);

void         valent_sms_conversation_scroll_to_date    (ValentSmsConversation *conversation,
                                                        gint64                 date);
void         valent_sms_conversation_scroll_to_message (ValentSmsConversation *conversation,
                                                        ValentMessage         *message);

G_END_DECLS
