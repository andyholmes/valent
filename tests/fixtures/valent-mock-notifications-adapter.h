// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <valent.h>

G_BEGIN_DECLS

#define VALENT_TYPE_MOCK_NOTIFICATIONS_ADAPTER (valent_mock_notifications_adapter_get_type())

G_DECLARE_FINAL_TYPE (ValentMockNotificationsAdapter, valent_mock_notifications_adapter, VALENT, MOCK_NOTIFICATIONS_ADAPTER, ValentNotificationsAdapter)

G_END_DECLS

