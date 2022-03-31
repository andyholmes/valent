// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_TEST_INSIDE) && !defined (VALENT_TEST_COMPILATION)
# error "Only <libvalent-test.h> can be included directly."
#endif

#include <libvalent-core.h>

G_BEGIN_DECLS

#define VALENT_TYPE_MOCK_APPLICATION_PLUGIN (valent_mock_application_plugin_get_type())

G_DECLARE_FINAL_TYPE (ValentMockApplicationPlugin, valent_mock_application_plugin, VALENT, MOCK_APPLICATION_PLUGIN, ValentApplicationPlugin)

G_END_DECLS

