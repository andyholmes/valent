// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <libvalent-power.h>

G_BEGIN_DECLS

#define VALENT_TYPE_TEST_POWER_DEVICE_PROVIDER (valent_test_power_device_provider_get_type ())

G_DECLARE_FINAL_TYPE (ValentTestPowerDeviceProvider, valent_test_power_device_provider, VALENT, TEST_POWER_DEVICE_PROVIDER, ValentPowerDeviceProvider)

G_END_DECLS

