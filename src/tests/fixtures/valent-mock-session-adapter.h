// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_TEST_INSIDE) && !defined (VALENT_TEST_COMPILATION)
# error "Only <libvalent-test.h> can be included directly."
#endif

#include <libvalent-session.h>

G_BEGIN_DECLS

#define VALENT_TYPE_MOCK_SESSION_ADAPTER (valent_mock_session_adapter_get_type())

G_DECLARE_FINAL_TYPE (ValentMockSessionAdapter, valent_mock_session_adapter, VALENT, MOCK_SESSION_ADAPTER, ValentSessionAdapter)

ValentSessionAdapter * valent_mock_session_adapter_get_instance (void);

G_END_DECLS

