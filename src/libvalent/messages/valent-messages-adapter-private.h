// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <tracker-sparql.h>

#include "valent-message.h"

G_BEGIN_DECLS

/*< private>
 *
 * Cursor columns for `vmo:PhoneMessage`.
 */
#define CURSOR_MESSAGE_IRI                0
#define CURSOR_MESSAGE_BOX                1
#define CURSOR_MESSAGE_DATE               2
#define CURSOR_MESSAGE_ID                 3
#define CURSOR_MESSAGE_READ               4
#define CURSOR_MESSAGE_RECIPIENTS         5
#define CURSOR_MESSAGE_SENDER             6
#define CURSOR_MESSAGE_SUBSCRIPTION_ID    7
#define CURSOR_MESSAGE_TEXT               8
#define CURSOR_MESSAGE_THREAD_ID          9
#define CURSOR_MESSAGE_ATTACHMENT_IRI     10
#define CURSOR_MESSAGE_ATTACHMENT_PREVIEW 11
#define CURSOR_MESSAGE_ATTACHMENT_FILE    12


ValentMessage * valent_message_from_sparql_cursor (TrackerSparqlCursor *cursor,
                                                   ValentMessage       *current);

G_END_DECLS

