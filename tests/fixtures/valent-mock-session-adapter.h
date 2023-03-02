// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <valent.h>

G_BEGIN_DECLS

#define VALENT_TYPE_MOCK_SESSION_ADAPTER (valent_mock_session_adapter_get_type())

G_DECLARE_FINAL_TYPE (ValentMockSessionAdapter, valent_mock_session_adapter, VALENT, MOCK_SESSION_ADAPTER, ValentSessionAdapter)

G_END_DECLS

