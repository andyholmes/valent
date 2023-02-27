// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <valent.h>

G_BEGIN_DECLS

#define VALENT_TYPE_MOCK_PREFERENCES_PAGE (valent_mock_preferences_page_get_type())

G_DECLARE_FINAL_TYPE (ValentMockPreferencesPage, valent_mock_preferences_page, VALENT, MOCK_PREFERENCES_PAGE, ValentPreferencesPage)

G_END_DECLS
