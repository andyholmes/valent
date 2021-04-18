// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_TEST_INSIDE) && !defined (VALENT_TEST_COMPILATION)
# error "Only <libvalent-test.h> can be included directly."
#endif

#include <libvalent-power.h>

G_BEGIN_DECLS

#define VALENT_TYPE_MOCK_POWER_DEVICE_PROVIDER (valent_mock_power_device_provider_get_type ())

G_DECLARE_FINAL_TYPE (ValentMockPowerDeviceProvider, valent_mock_power_device_provider, VALENT, MOCK_POWER_DEVICE_PROVIDER, ValentPowerDeviceProvider)

G_END_DECLS

