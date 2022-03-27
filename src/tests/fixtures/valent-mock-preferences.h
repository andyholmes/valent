// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <libvalent-ui.h>

G_BEGIN_DECLS

#define VALENT_TYPE_MOCK_PREFERENCES (valent_mock_preferences_get_type())

G_DECLARE_FINAL_TYPE (ValentMockPreferences, valent_mock_preferences, VALENT, MOCK_PREFERENCES, AdwPreferencesPage)

G_END_DECLS
