// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <gio/gio.h>

G_BEGIN_DECLS

#define VALENT_TYPE_MOCK_NETWORK_MONITOR (valent_mock_network_monitor_get_type())

G_DECLARE_FINAL_TYPE (ValentMockNetworkMonitor, valent_mock_network_monitor, VALENT, MOCK_NETWORK_MONITOR, GObject)

G_END_DECLS

