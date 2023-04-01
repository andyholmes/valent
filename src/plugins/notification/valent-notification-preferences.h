// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <valent.h>

G_BEGIN_DECLS

#define VALENT_TYPE_NOTIFICATION_PREFERENCES (valent_notification_preferences_get_type())

G_DECLARE_FINAL_TYPE (ValentNotificationPreferences, valent_notification_preferences, VALENT, NOTIFICATION_PREFERENCES, ValentDevicePreferencesGroup)

G_END_DECLS
