// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <valent.h>

G_BEGIN_DECLS

#define VALENT_TYPE_MOCK_DEVICE_PREFERENCES_GROUP (valent_mock_device_preferencs_group_get_type())

G_DECLARE_FINAL_TYPE (ValentMockDevicePreferencesGroup, valent_mock_device_preferencs_group, VALENT, MOCK_DEVICE_PREFERENCES_GROUP, ValentDevicePreferencesGroup)

G_END_DECLS
