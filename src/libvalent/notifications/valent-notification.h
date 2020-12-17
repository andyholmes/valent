// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_NOTIFICATIONS_INSIDE) && !defined (VALENT_NOTIFICATIONS_COMPILATION)
# error "Only <libvalent-notifications.h> can be included directly."
#endif

#include <gio/gio.h>

G_BEGIN_DECLS

#define VALENT_TYPE_NOTIFICATION (valent_notification_get_type())

G_DECLARE_FINAL_TYPE (ValentNotification, valent_notification, VALENT, NOTIFICATION, GObject)

const char            * valent_notification_get_application        (ValentNotification    *notification);
void                    valent_notification_set_application        (ValentNotification    *notification,
                                                                    const char            *name);
const char            * valent_notification_get_body               (ValentNotification    *notification);
void                    valent_notification_set_body               (ValentNotification    *notification,
                                                                    const char            *body);
GIcon                 * valent_notification_get_icon               (ValentNotification    *notification);
void                    valent_notification_set_icon               (ValentNotification    *notification,
                                                                    GIcon                 *icon);
void                    valent_notification_set_icon_from_string   (ValentNotification    *notification,
                                                                    const char            *icon_name);
const char            * valent_notification_get_id                 (ValentNotification    *notification);
void                    valent_notification_set_id                 (ValentNotification    *notification,
                                                                    const char            *id);
GNotificationPriority   valent_notification_get_priority           (ValentNotification    *notification);
void                    valent_notification_set_priority           (ValentNotification    *notification,
                                                                    GNotificationPriority  priority);
gint64                  valent_notification_get_time               (ValentNotification    *notification);
void                    valent_notification_set_time               (ValentNotification    *notification,
                                                                    gint64                 time);
const char            * valent_notification_get_title              (ValentNotification    *notification);
void                    valent_notification_set_title              (ValentNotification    *notification,
                                                                    const char            *title);

void                    valent_notification_set_action             (ValentNotification    *notification,
                                                                    const char            *action);
void                    valent_notification_set_action_and_target  (ValentNotification    *notification,
                                                                    const char            *action,
                                                                    GVariant              *target);
void                    valent_notification_add_button             (ValentNotification    *notification,
                                                                    const char            *label,
                                                                    const char            *action);
void                    valent_notification_add_button_with_target (ValentNotification    *notification,
                                                                    const char            *label,
                                                                    const char            *action,
                                                                    GVariant              *target);

ValentNotification    * valent_notification_new                    (const char            *title);
GVariant              * valent_notification_serialize              (ValentNotification    *notification);
ValentNotification    * valent_notification_deserialize            (GVariant              *variant);

G_END_DECLS
