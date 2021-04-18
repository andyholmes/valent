// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_TEST_INSIDE) && !defined (VALENT_TEST_COMPILATION)
# error "Only <libvalent-test.h> can be included directly."
#endif

#include <libvalent-notifications.h>

G_BEGIN_DECLS

#define VALENT_TYPE_MOCK_NOTIFICATION_SOURCE (valent_mock_notification_source_get_type())

G_DECLARE_FINAL_TYPE (ValentMockNotificationSource, valent_mock_notification_source, VALENT, MOCK_NOTIFICATION_SOURCE, ValentNotificationSource)

ValentNotificationSource * valent_mock_notification_source_get_instance (void);

G_END_DECLS

