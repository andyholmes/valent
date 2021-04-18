// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_TEST_INSIDE) && !defined (VALENT_TEST_COMPILATION)
# error "Only <libvalent-test.h> can be included directly."
#endif

#include <libvalent-power.h>

G_BEGIN_DECLS

#define VALENT_TYPE_MOCK_POWER_DEVICE (valent_mock_power_device_get_type ())

G_DECLARE_FINAL_TYPE (ValentMockPowerDevice, valent_mock_power_device, VALENT, MOCK_POWER_DEVICE, ValentPowerDevice)

void valent_mock_power_device_set_kind    (ValentMockPowerDevice *self,
                                           ValentPowerKind        kind);
void valent_mock_power_device_set_level   (ValentMockPowerDevice *self,
                                           int                    level);
void valent_mock_power_device_set_state   (ValentMockPowerDevice *self,
                                           ValentPowerState       state);
void valent_mock_power_device_set_warning (ValentMockPowerDevice *self,
                                           ValentPowerWarning     warning);

G_END_DECLS

