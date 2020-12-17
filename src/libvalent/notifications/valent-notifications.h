// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_NOTIFICATIONS_INSIDE) && !defined (VALENT_NOTIFICATIONS_COMPILATION)
# error "Only <libvalent-notifications.h> can be included directly."
#endif

#include <gio/gio.h>
#include <libvalent-core.h>

G_BEGIN_DECLS

#define VALENT_TYPE_NOTIFICATIONS (valent_notifications_get_type ())

G_DECLARE_FINAL_TYPE (ValentNotifications, valent_notifications, VALENT, NOTIFICATIONS, ValentComponent)

ValentNotifications * valent_notifications_get_default      (void);
GVariant            * valent_notifications_get_applications (ValentNotifications *notifications);

G_END_DECLS

