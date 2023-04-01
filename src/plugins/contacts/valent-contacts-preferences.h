// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <valent.h>

G_BEGIN_DECLS

#define VALENT_TYPE_CONTACTS_PREFERENCES (valent_contacts_preferences_get_type())

G_DECLARE_FINAL_TYPE (ValentContactsPreferences, valent_contacts_preferences, VALENT, CONTACTS_PREFERENCES, ValentDevicePreferencesGroup)

G_END_DECLS
