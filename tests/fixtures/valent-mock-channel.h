// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <libvalent-device.h>

G_BEGIN_DECLS

#define VALENT_TYPE_MOCK_CHANNEL (valent_mock_channel_get_type())

G_DECLARE_FINAL_TYPE (ValentMockChannel, valent_mock_channel, VALENT, MOCK_CHANNEL, ValentChannel)

G_END_DECLS

