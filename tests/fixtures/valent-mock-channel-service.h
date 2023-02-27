// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <valent.h>

G_BEGIN_DECLS

#define VALENT_TYPE_MOCK_CHANNEL_SERVICE (valent_mock_channel_service_get_type())

G_DECLARE_FINAL_TYPE (ValentMockChannelService, valent_mock_channel_service, VALENT, MOCK_CHANNEL_SERVICE, ValentChannelService)

ValentChannelService * valent_mock_channel_service_get_instance (void);
ValentChannel        * valent_mock_channel_service_get_channel  (void);
ValentChannel        * valent_mock_channel_service_get_endpoint (void);

G_END_DECLS

