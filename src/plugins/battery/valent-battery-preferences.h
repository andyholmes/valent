// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <gtk/gtk.h>
#include <libvalent-core.h>
#include <libvalent-ui.h>

G_BEGIN_DECLS

#define VALENT_TYPE_BATTERY_PREFERENCES (valent_battery_preferences_get_type())

G_DECLARE_FINAL_TYPE (ValentBatteryPreferences, valent_battery_preferences, VALENT, BATTERY_PREFERENCES, AdwPreferencesPage)

G_END_DECLS
