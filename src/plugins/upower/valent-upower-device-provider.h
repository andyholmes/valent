// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <libvalent-power.h>

G_BEGIN_DECLS

#define VALENT_TYPE_UPOWER_DEVICE_PROVIDER (valent_upower_device_provider_get_type ())

G_DECLARE_FINAL_TYPE (ValentUPowerDeviceProvider, valent_upower_device_provider, VALENT, UPOWER_DEVICE_PROVIDER, ValentPowerDeviceProvider)

G_END_DECLS

