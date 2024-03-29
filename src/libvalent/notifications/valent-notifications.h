// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_INSIDE) && !defined (VALENT_COMPILATION)
# error "Only <valent.h> can be included directly."
#endif

#include "../core/valent-component.h"

G_BEGIN_DECLS

#define VALENT_TYPE_NOTIFICATIONS (valent_notifications_get_type ())

VALENT_AVAILABLE_IN_1_0
G_DECLARE_FINAL_TYPE (ValentNotifications, valent_notifications, VALENT, NOTIFICATIONS, ValentComponent)

VALENT_AVAILABLE_IN_1_0
ValentNotifications * valent_notifications_get_default      (void);

VALENT_AVAILABLE_IN_1_0
GVariant            * valent_notifications_get_applications (ValentNotifications *notifications);

G_END_DECLS

