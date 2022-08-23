// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <libvalent-core.h>

G_BEGIN_DECLS

#define VALENT_TYPE_MOCK_DEVICE_PLUGIN (valent_mock_device_plugin_get_type())

G_DECLARE_FINAL_TYPE (ValentMockDevicePlugin, valent_mock_device_plugin, VALENT, MOCK_DEVICE_PLUGIN, ValentDevicePlugin)

G_END_DECLS

