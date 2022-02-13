// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <gtk/gtk.h>
#include <libvalent-notifications.h>

G_BEGIN_DECLS

#define VALENT_TYPE_NOTIFICATION_DIALOG (valent_notification_dialog_get_type())

G_DECLARE_FINAL_TYPE (ValentNotificationDialog, valent_notification_dialog, VALENT, NOTIFICATION_DIALOG, GtkDialog)

GtkDialog          * valent_notification_dialog_new              (ValentNotification       *notification);
ValentNotification * valent_notification_dialog_get_notification (ValentNotificationDialog *dialog);
char               * valent_notification_dialog_get_reply        (ValentNotificationDialog *dialog);
const char         * valent_notification_dialog_get_reply_id     (ValentNotificationDialog *dialog);
void                 valent_notification_dialog_set_reply_id     (ValentNotificationDialog *dialog,
                                                                  const char               *reply_id);
void                 valent_notification_dialog_update_state     (ValentNotificationDialog *dialog,
                                                                  gboolean                  state);


G_END_DECLS
