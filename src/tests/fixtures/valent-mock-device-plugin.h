// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_TEST_INSIDE) && !defined (VALENT_TEST_COMPILATION)
# error "Only <libvalent-test.h> can be included directly."
#endif

#include <libpeas/peas.h>

G_BEGIN_DECLS

#define VALENT_TYPE_MOCK_DEVICE_PLUGIN (valent_mock_device_plugin_get_type())

G_DECLARE_FINAL_TYPE (ValentMockDevicePlugin, valent_mock_device_plugin, VALENT, MOCK_DEVICE_PLUGIN, PeasExtensionBase)

G_END_DECLS

