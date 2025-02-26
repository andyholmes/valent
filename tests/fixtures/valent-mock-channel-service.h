// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <valent.h>

G_BEGIN_DECLS

#define VALENT_TYPE_MOCK_CHANNEL_SERVICE (valent_mock_channel_service_get_type())

G_DECLARE_FINAL_TYPE (ValentMockChannelService, valent_mock_channel_service, VALENT, MOCK_CHANNEL_SERVICE, ValentChannelService)

G_END_DECLS

