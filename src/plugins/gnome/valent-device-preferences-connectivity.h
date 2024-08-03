// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include "valent-device-preferences-group.h"

G_BEGIN_DECLS

#define VALENT_TYPE_CONNECTIVITY_REPORT_PREFERENCES (valent_connectivity_report_preferences_get_type())

G_DECLARE_FINAL_TYPE (ValentConnectivityReportPreferences, valent_connectivity_report_preferences, VALENT, CONNECTIVITY_REPORT_PREFERENCES, ValentDevicePreferencesGroup)

G_END_DECLS
