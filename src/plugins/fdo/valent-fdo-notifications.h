// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <valent.h>

G_BEGIN_DECLS

#define VALENT_TYPE_FDO_NOTIFICATIONS (valent_fdo_notifications_get_type())

G_DECLARE_FINAL_TYPE (ValentFdoNotifications, valent_fdo_notifications, VALENT, FDO_NOTIFICATIONS, ValentNotificationsAdapter)

G_END_DECLS

