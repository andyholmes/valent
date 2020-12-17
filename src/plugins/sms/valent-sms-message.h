// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <gio/gio.h>

G_BEGIN_DECLS

/**
 * ValentSmsMessageBox:
 * @VALENT_SMS_BOX_ALL: All messages
 * @VALENT_SMS_BOX_INBOX: Received incoming messages
 * @VALENT_SMS_BOX_SENT: Sent outgoing messages
 * @VALENT_SMS_BOX_DRAFTS: Unfinished outgoing messages
 * @VALENT_SMS_BOX_OUTBOX: Pending outgoing messages
 * @VALENT_SMS_BOX_FAILED: Failed outgoing messages
 *
 * Enumeration of message types. These are the same as those used in Android,
 * however only @VALENT_SMS_BOX_SENT and @VALENT_SMS_BOX_INBOX are used
 * currently, and they are read-only.
 *
 * See also:
 * - https://developer.android.com/reference/android/provider/Telephony.TextBasedSmsColumns.html
 * - https://developer.android.com/reference/android/provider/Telephony.BaseMmsColumns.html
 */
typedef enum
{
  VALENT_SMS_MESSAGE_BOX_ALL,
  VALENT_SMS_MESSAGE_BOX_INBOX,
  VALENT_SMS_MESSAGE_BOX_SENT,
  VALENT_SMS_MESSAGE_BOX_DRAFTS,
  VALENT_SMS_MESSAGE_BOX_OUTBOX,
  VALENT_SMS_MESSAGE_BOX_FAILED
} ValentSmsMessageBox;


#define VALENT_TYPE_SMS_MESSAGE (valent_sms_message_get_type())

G_DECLARE_FINAL_TYPE (ValentSmsMessage, valent_sms_message, VALENT, SMS_MESSAGE, GObject)

ValentSmsMessage    * valent_sms_message_new           (gint64               date,
                                                        const char          *media,
                                                        const char          *text);
ValentSmsMessageBox   valent_sms_message_get_box       (ValentSmsMessage    *message);
void                  valent_sms_message_set_box       (ValentSmsMessage    *message,
                                                        ValentSmsMessageBox  box);
gint64                valent_sms_message_get_date      (ValentSmsMessage    *message);
void                  valent_sms_message_set_date      (ValentSmsMessage    *message,
                                                        gint64               date);
gint64                valent_sms_message_get_id        (ValentSmsMessage    *message);
void                  valent_sms_message_set_id        (ValentSmsMessage    *message,
                                                        gint64               id);
GVariant            * valent_sms_message_get_metadata  (ValentSmsMessage    *message);
void                  valent_sms_message_set_metadata  (ValentSmsMessage    *message,
                                                       GVariant            *metadata);
gboolean              valent_sms_message_get_read      (ValentSmsMessage    *message);
void                  valent_sms_message_set_read      (ValentSmsMessage    *message,
                                                        gboolean             read);
const char          * valent_sms_message_get_sender    (ValentSmsMessage    *message);
void                  valent_sms_message_set_sender    (ValentSmsMessage    *message,
                                                        const char          *sender);
const char          * valent_sms_message_get_text      (ValentSmsMessage    *message);
void                  valent_sms_message_set_text      (ValentSmsMessage    *message,
                                                        const char          *text);
gint64                valent_sms_message_get_thread_id (ValentSmsMessage    *message);
void                  valent_sms_message_set_thread_id (ValentSmsMessage    *message,
                                                        gint64               thread_id);

G_END_DECLS
