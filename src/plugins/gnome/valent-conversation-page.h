// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <adwaita.h>
#include <libebook-contacts/libebook-contacts.h>
#include <valent.h>

G_BEGIN_DECLS

#define VALENT_TYPE_CONVERSATION_PAGE (valent_conversation_page_get_type())

G_DECLARE_FINAL_TYPE (ValentConversationPage, valent_conversation_page, VALENT, CONVERSATION_PAGE, AdwNavigationPage)

void         valent_conversation_page_add_participant   (ValentConversationPage *conversation,
                                                         EContact               *contact,
                                                         const char             *medium);
const char * valent_conversation_page_get_iri           (ValentConversationPage *conversation);
void         valent_conversation_page_scroll_to_date    (ValentConversationPage *conversation,
                                                         int64_t                 date);
void         valent_conversation_page_scroll_to_message (ValentConversationPage *conversation,
                                                         ValentMessage          *message);

G_END_DECLS
