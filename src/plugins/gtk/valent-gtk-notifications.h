// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <glib-object.h>
#include <libvalent-notifications.h>

G_BEGIN_DECLS

#define VALENT_TYPE_GTK_NOTIFICATIONS (valent_gtk_notifications_get_type())

G_DECLARE_FINAL_TYPE (ValentGtkNotifications, valent_gtk_notifications, VALENT, GTK_NOTIFICATIONS, ValentNotificationsAdapter)

G_END_DECLS

