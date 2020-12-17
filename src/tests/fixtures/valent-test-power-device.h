// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <libvalent-power.h>

G_BEGIN_DECLS

#define VALENT_TYPE_TEST_POWER_DEVICE (valent_test_power_device_get_type ())

G_DECLARE_FINAL_TYPE (ValentTestPowerDevice, valent_test_power_device, VALENT, TEST_POWER_DEVICE, ValentPowerDevice)

void valent_test_power_device_set_kind    (ValentTestPowerDevice *self,
                                           ValentPowerKind        kind);
void valent_test_power_device_set_level   (ValentTestPowerDevice *self,
                                           int                    level);
void valent_test_power_device_set_state   (ValentTestPowerDevice *self,
                                           ValentPowerState       state);
void valent_test_power_device_set_warning (ValentTestPowerDevice *self,
                                           ValentPowerWarning     warning);

G_END_DECLS

