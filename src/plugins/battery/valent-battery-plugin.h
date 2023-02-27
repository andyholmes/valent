// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <valent.h>

G_BEGIN_DECLS

#define VALENT_TYPE_BATTERY_PLUGIN (valent_battery_plugin_get_type())

G_DECLARE_FINAL_TYPE (ValentBatteryPlugin, valent_battery_plugin, VALENT, BATTERY_PLUGIN, ValentDevicePlugin)

G_END_DECLS

