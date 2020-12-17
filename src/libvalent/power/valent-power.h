// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_POWER_INSIDE) && !defined (VALENT_POWER_COMPILATION)
# error "Only <libvalent-power.h> can be included directly."
#endif

#include <libvalent-core.h>

#include "valent-power-device.h"
#include "valent-power-enums.h"

G_BEGIN_DECLS

#define VALENT_TYPE_POWER (valent_power_get_type ())

G_DECLARE_FINAL_TYPE (ValentPower, valent_power, VALENT, POWER, ValentComponent)

ValentPower        * valent_power_get_default          (void);

gboolean             valent_power_get_battery_charging (ValentPower *power);
int                  valent_power_get_battery_level    (ValentPower *power);
unsigned int         valent_power_get_battery_state    (ValentPower *power);
ValentPowerWarning   valent_power_get_battery_warning  (ValentPower *power);

G_END_DECLS

