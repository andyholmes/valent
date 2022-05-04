// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_NOTIFICATIONS_INSIDE) && !defined (VALENT_NOTIFICATIONS_COMPILATION)
# error "Only <libvalent-notifications.h> can be included directly."
#endif

#include <libvalent-core.h>

G_BEGIN_DECLS

#define VALENT_TYPE_NOTIFICATION (valent_notification_get_type())

VALENT_AVAILABLE_IN_1_0
G_DECLARE_FINAL_TYPE (ValentNotification, valent_notification, VALENT, NOTIFICATION, GObject)

VALENT_AVAILABLE_IN_1_0
ValentNotification    * valent_notification_new                    (const char            *title);
VALENT_AVAILABLE_IN_1_0
const char            * valent_notification_get_application        (ValentNotification    *notification);
VALENT_AVAILABLE_IN_1_0
void                    valent_notification_set_application        (ValentNotification    *notification,
                                                                    const char            *application);
VALENT_AVAILABLE_IN_1_0
const char            * valent_notification_get_body               (ValentNotification    *notification);
VALENT_AVAILABLE_IN_1_0
void                    valent_notification_set_body               (ValentNotification    *notification,
                                                                    const char            *body);
VALENT_AVAILABLE_IN_1_0
GIcon                 * valent_notification_get_icon               (ValentNotification    *notification);
VALENT_AVAILABLE_IN_1_0
void                    valent_notification_set_icon               (ValentNotification    *notification,
                                                                    GIcon                 *icon);
VALENT_AVAILABLE_IN_1_0
const char            * valent_notification_get_id                 (ValentNotification    *notification);
VALENT_AVAILABLE_IN_1_0
void                    valent_notification_set_id                 (ValentNotification    *notification,
                                                                    const char            *id);
VALENT_AVAILABLE_IN_1_0
GNotificationPriority   valent_notification_get_priority           (ValentNotification    *notification);
VALENT_AVAILABLE_IN_1_0
void                    valent_notification_set_priority           (ValentNotification    *notification,
                                                                    GNotificationPriority  priority);
VALENT_AVAILABLE_IN_1_0
gint64                  valent_notification_get_time               (ValentNotification    *notification);
VALENT_AVAILABLE_IN_1_0
void                    valent_notification_set_time               (ValentNotification    *notification,
                                                                    gint64                 time);
VALENT_AVAILABLE_IN_1_0
const char            * valent_notification_get_title              (ValentNotification    *notification);
VALENT_AVAILABLE_IN_1_0
void                    valent_notification_set_title              (ValentNotification    *notification,
                                                                    const char            *title);

VALENT_AVAILABLE_IN_1_0
void                    valent_notification_set_action             (ValentNotification    *notification,
                                                                    const char            *action);
VALENT_AVAILABLE_IN_1_0
void                    valent_notification_set_action_and_target  (ValentNotification    *notification,
                                                                    const char            *action,
                                                                    GVariant              *target);
VALENT_AVAILABLE_IN_1_0
void                    valent_notification_add_button             (ValentNotification    *notification,
                                                                    const char            *label,
                                                                    const char            *action);
VALENT_AVAILABLE_IN_1_0
void                    valent_notification_add_button_with_target (ValentNotification    *notification,
                                                                    const char            *label,
                                                                    const char            *action,
                                                                    GVariant              *target);
VALENT_AVAILABLE_IN_1_0
GVariant              * valent_notification_serialize              (ValentNotification    *notification);
VALENT_AVAILABLE_IN_1_0
ValentNotification    * valent_notification_deserialize            (GVariant              *variant);
VALENT_AVAILABLE_IN_1_0
unsigned int            valent_notification_hash                   (gconstpointer          notification);
VALENT_AVAILABLE_IN_1_0
gboolean                valent_notification_equal                  (gconstpointer          notification1,
                                                                    gconstpointer          notification2);

G_END_DECLS
