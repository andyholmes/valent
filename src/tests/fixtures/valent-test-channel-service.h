// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_TEST_INSIDE) && !defined (VALENT_TEST_COMPILATION)
# error "Only <libvalent-test.h> can be included directly."
#endif

#include <libvalent-core.h>

G_BEGIN_DECLS

#define VALENT_TYPE_TEST_CHANNEL_SERVICE (valent_test_channel_service_get_type())

G_DECLARE_FINAL_TYPE (ValentTestChannelService, valent_test_channel_service, VALENT, TEST_CHANNEL_SERVICE, ValentChannelService)

ValentChannelService * valent_test_channel_service_get_instance (void);
ValentChannel        * valent_test_channel_service_get_channel  (void);
ValentChannel        * valent_test_channel_service_get_endpoint (void);

G_END_DECLS

