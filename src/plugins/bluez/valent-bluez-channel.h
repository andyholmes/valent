// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <libvalent-device.h>

G_BEGIN_DECLS

#define VALENT_TYPE_BLUEZ_CHANNEL (valent_bluez_channel_get_type())

G_DECLARE_FINAL_TYPE (ValentBluezChannel, valent_bluez_channel, VALENT, BLUEZ_CHANNEL, ValentChannel)

const char * valent_bluez_channel_get_uuid (ValentBluezChannel *self);

G_END_DECLS

