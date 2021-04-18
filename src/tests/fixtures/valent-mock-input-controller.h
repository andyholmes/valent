// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_TEST_INSIDE) && !defined (VALENT_TEST_COMPILATION)
# error "Only <libvalent-test.h> can be included directly."
#endif

#include <libpeas/peas.h>
#include <libvalent-input.h>

G_BEGIN_DECLS

#define VALENT_TYPE_MOCK_INPUT_CONTROLLER (valent_mock_input_controller_get_type())

G_DECLARE_FINAL_TYPE (ValentMockInputController, valent_mock_input_controller, VALENT, MOCK_INPUT_CONTROLLER, PeasExtensionBase)

G_END_DECLS

