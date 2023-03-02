// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <valent.h>

G_BEGIN_DECLS

#define VALENT_TYPE_MOCK_DEVICE_PREFERENCES_PAGE (valent_mock_device_preferences_page_get_type())

G_DECLARE_FINAL_TYPE (ValentMockDevicePreferencesPage, valent_mock_device_preferences_page, VALENT, MOCK_DEVICE_PREFERENCES_PAGE, ValentDevicePreferencesPage)

G_END_DECLS
