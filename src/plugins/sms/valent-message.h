// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <gio/gio.h>

G_BEGIN_DECLS

/**
 * ValentMessageBox:
 * @VALENT_MESSAGE_BOX_ALL: All messages
 * @VALENT_MESSAGE_BOX_INBOX: Received incoming messages
 * @VALENT_MESSAGE_BOX_SENT: Sent outgoing messages
 * @VALENT_MESSAGE_BOX_DRAFTS: Unfinished outgoing messages
 * @VALENT_MESSAGE_BOX_OUTBOX: Pending outgoing messages
 * @VALENT_MESSAGE_BOX_FAILED: Failed outgoing messages
 *
 * Enumeration of message types. These are the same as those used in Android,
 * however only @VALENT_MESSAGE_BOX_SENT and @VALENT_MESSAGE_BOX_INBOX are used
 * currently.
 *
 * See also:
 * - https://developer.android.com/reference/android/provider/Telephony.TextBasedSmsColumns.html
 * - https://developer.android.com/reference/android/provider/Telephony.BaseMmsColumns.html
 */
typedef enum
{
  VALENT_MESSAGE_BOX_ALL,
  VALENT_MESSAGE_BOX_INBOX,
  VALENT_MESSAGE_BOX_SENT,
  VALENT_MESSAGE_BOX_DRAFTS,
  VALENT_MESSAGE_BOX_OUTBOX,
  VALENT_MESSAGE_BOX_FAILED
} ValentMessageBox;

/**
 * ValentMessageFlags:
 * @VALENT_MESSAGE_FLAGS_NONE: The message has no content
 * @VALENT_MESSAGE_FLAGS_TEXT: The message has text content
 * @VALENT_MESSAGE_FLAGS_MULTI: The message has multiple recipients
 *
 * Flags for message events.
 *
 * See also:
 * - https://invent.kde.org/network/kdeconnect-android/-/blob/master/src/org/kde/kdeconnect/Helpers/SMSHelper.java
 * - https://invent.kde.org/network/kdeconnect-android/-/blob/master/src/org/kde/kdeconnect/Plugins/SMSPlugin/SMSPlugin.java
 */
typedef enum
{
  VALENT_MESSAGE_FLAGS_UNKNOWN,
  VALENT_MESSAGE_FLAGS_TEXT = 1 << 0,
  VALENT_MESSAGE_FLAGS_MULTI = 1 << 1,
} ValentMessageFlags;


#define VALENT_TYPE_MESSAGE (valent_message_get_type())

G_DECLARE_FINAL_TYPE (ValentMessage, valent_message, VALENT, MESSAGE, GObject)

ValentMessageBox   valent_message_get_box       (ValentMessage *message);
gint64             valent_message_get_date      (ValentMessage *message);
gint64             valent_message_get_id        (ValentMessage *message);
GVariant         * valent_message_get_metadata  (ValentMessage *message);
gboolean           valent_message_get_read      (ValentMessage *message);
void               valent_message_set_read      (ValentMessage *message,
                                                 gboolean       read);
const char       * valent_message_get_sender    (ValentMessage *message);
const char       * valent_message_get_text      (ValentMessage *message);
gint64             valent_message_get_thread_id (ValentMessage *message);
void               valent_message_update        (ValentMessage *message,
                                                 ValentMessage *update);

G_END_DECLS
