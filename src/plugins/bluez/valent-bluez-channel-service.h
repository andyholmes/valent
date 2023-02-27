// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <valent.h>

G_BEGIN_DECLS

#define VALENT_TYPE_BLUEZ_CHANNEL_SERVICE (valent_bluez_channel_service_get_type())

G_DECLARE_FINAL_TYPE (ValentBluezChannelService, valent_bluez_channel_service, VALENT, BLUEZ_CHANNEL_SERVICE, ValentChannelService)

G_END_DECLS

