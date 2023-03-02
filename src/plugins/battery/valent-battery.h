// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <gio/gio.h>

G_BEGIN_DECLS

#define VALENT_TYPE_BATTERY (valent_battery_get_type())

G_DECLARE_FINAL_TYPE (ValentBattery, valent_battery, VALENT, BATTERY, GObject)

ValentBattery * valent_battery_get_default     (void);
int             valent_battery_current_charge  (ValentBattery *battery);
gboolean        valent_battery_is_charging     (ValentBattery *battery);
unsigned int    valent_battery_threshold_event (ValentBattery *battery);

G_END_DECLS

