// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_INSIDE) && !defined (VALENT_COMPILATION)
# error "Only <valent.h> can be included directly."
#endif

#include <gio/gio.h>

#include "../core/valent-object.h"

G_BEGIN_DECLS

/**
 * ValentMessageBox:
 * @VALENT_MESSAGE_BOX_ALL: All messages
 * @VALENT_MESSAGE_BOX_INBOX: Received incoming messages
 * @VALENT_MESSAGE_BOX_SENT: Sent outgoing messages
 * @VALENT_MESSAGE_BOX_DRAFTS: Unfinished outgoing messages
 * @VALENT_MESSAGE_BOX_OUTBOX: Pending outgoing messages
 * @VALENT_MESSAGE_BOX_FAILED: Failed outgoing messages
 * @VALENT_MESSAGE_BOX_QUEUED: Queued outgoing messages
 *
 * Enumeration of message types. These are the same as those used in Android,
 * however only @VALENT_MESSAGE_BOX_SENT and @VALENT_MESSAGE_BOX_INBOX are used
 * currently.
 *
 * See also:
 * - https://developer.android.com/reference/android/provider/Telephony.TextBasedSmsColumns.html
 * - https://developer.android.com/reference/android/provider/Telephony.BaseMmsColumns.html
 *
 * Since: 1.0
 */
typedef enum
{
  VALENT_MESSAGE_BOX_ALL,
  VALENT_MESSAGE_BOX_INBOX,
  VALENT_MESSAGE_BOX_SENT,
  VALENT_MESSAGE_BOX_DRAFTS,
  VALENT_MESSAGE_BOX_OUTBOX,
  VALENT_MESSAGE_BOX_FAILED,
  VALENT_MESSAGE_BOX_QUEUED,
} ValentMessageBox;

#define VALENT_TYPE_MESSAGE (valent_message_get_type())

VALENT_AVAILABLE_IN_1_0
G_DECLARE_FINAL_TYPE (ValentMessage, valent_message, VALENT, MESSAGE, ValentObject)

VALENT_AVAILABLE_IN_1_0
GListModel         * valent_message_get_attachments     (ValentMessage *message);
VALENT_AVAILABLE_IN_1_0
ValentMessageBox     valent_message_get_box             (ValentMessage *message);
VALENT_AVAILABLE_IN_1_0
int64_t              valent_message_get_date            (ValentMessage *message);
VALENT_AVAILABLE_IN_1_0
int64_t              valent_message_get_id              (ValentMessage *message);
VALENT_AVAILABLE_IN_1_0
gboolean             valent_message_get_read            (ValentMessage *message);
VALENT_AVAILABLE_IN_1_0
const char * const * valent_message_get_recipients      (ValentMessage *message);
VALENT_AVAILABLE_IN_1_0
const char         * valent_message_get_sender          (ValentMessage *message);
VALENT_AVAILABLE_IN_1_0
int64_t              valent_message_get_subscription_id (ValentMessage *message);
VALENT_AVAILABLE_IN_1_0
const char         * valent_message_get_text            (ValentMessage *message);
VALENT_AVAILABLE_IN_1_0
int64_t              valent_message_get_thread_id       (ValentMessage *message);
VALENT_AVAILABLE_IN_1_0
void                 valent_message_update              (ValentMessage *message,
                                                         ValentMessage *update);

G_END_DECLS
