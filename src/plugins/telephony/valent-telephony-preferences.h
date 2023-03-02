// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <valent.h>

G_BEGIN_DECLS

#define VALENT_TYPE_TELEPHONY_PREFERENCES (valent_telephony_preferences_get_type())

G_DECLARE_FINAL_TYPE (ValentTelephonyPreferences, valent_telephony_preferences, VALENT, TELEPHONY_PREFERENCES, ValentDevicePreferencesPage)

G_END_DECLS
