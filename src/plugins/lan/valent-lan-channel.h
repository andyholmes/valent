// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <valent.h>

G_BEGIN_DECLS

#define VALENT_TYPE_LAN_CHANNEL (valent_lan_channel_get_type())

G_DECLARE_FINAL_TYPE (ValentLanChannel, valent_lan_channel, VALENT, LAN_CHANNEL, ValentChannel)

G_END_DECLS

