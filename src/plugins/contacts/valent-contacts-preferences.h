// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <libvalent-ui.h>

G_BEGIN_DECLS

#define VALENT_TYPE_CONTACTS_PREFERENCES (valent_contacts_preferences_get_type())

G_DECLARE_FINAL_TYPE (ValentContactsPreferences, valent_contacts_preferences, VALENT, CONTACTS_PREFERENCES, ValentDevicePreferencesPage)

G_END_DECLS
