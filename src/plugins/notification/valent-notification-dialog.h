// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <valent.h>

G_BEGIN_DECLS

#define VALENT_TYPE_NOTIFICATION_DIALOG (valent_notification_dialog_get_type())

G_DECLARE_FINAL_TYPE (ValentNotificationDialog, valent_notification_dialog, VALENT, NOTIFICATION_DIALOG, GtkWindow)

ValentNotification * valent_notification_dialog_get_notification (ValentNotificationDialog *dialog);
const char         * valent_notification_dialog_get_reply_id     (ValentNotificationDialog *dialog);
void                 valent_notification_dialog_set_reply_id     (ValentNotificationDialog *dialog,
                                                                  const char               *reply_id);

G_END_DECLS
