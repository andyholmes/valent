// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <libvalent-ui.h>

G_BEGIN_DECLS

#define VALENT_TYPE_BATTERY_GADGET (valent_battery_gadget_get_type())

G_DECLARE_FINAL_TYPE (ValentBatteryGadget, valent_battery_gadget, VALENT, BATTERY_GADGET, ValentDeviceGadget)

G_END_DECLS
