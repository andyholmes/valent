// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <libvalent-input.h>

G_BEGIN_DECLS

#define VALENT_TYPE_MOCK_INPUT_ADAPTER (valent_mock_input_adapter_get_type())

G_DECLARE_FINAL_TYPE (ValentMockInputAdapter, valent_mock_input_adapter, VALENT, MOCK_INPUT_ADAPTER, ValentInputAdapter)

G_END_DECLS

