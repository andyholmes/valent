// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <gtk/gtk.h>
#include <libvalent-notifications.h>

G_BEGIN_DECLS

#define VALENT_TYPE_NOTIFICATION_REPLY_DIALOG (valent_notification_reply_dialog_get_type())

G_DECLARE_FINAL_TYPE (ValentNotificationReplyDialog, valent_notification_reply_dialog, VALENT, NOTIFICATION_REPLY_DIALOG, GtkDialog)

GtkDialog  * valent_notification_reply_dialog_new      (ValentNotification            *notification);

char       * valent_notification_reply_dialog_get_text (ValentNotificationReplyDialog *dialog);
void         valent_notification_reply_dialog_set_text (ValentNotificationReplyDialog *dialog,
                                                        const char                    *text);
const char * valent_notification_reply_dialog_get_uuid (ValentNotificationReplyDialog *dialog);
void         valent_notification_reply_dialog_set_uuid (ValentNotificationReplyDialog *dialog,
                                                        const char                    *uuid);


G_END_DECLS
