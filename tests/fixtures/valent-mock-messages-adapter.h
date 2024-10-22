// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <valent.h>

G_BEGIN_DECLS

#define VALENT_TYPE_MOCK_MESSAGES_ADAPTER (valent_mock_messages_adapter_get_type ())

G_DECLARE_FINAL_TYPE (ValentMockMessagesAdapter, valent_mock_messages_adapter, VALENT, MOCK_MESSAGES_ADAPTER, ValentMessagesAdapter)

G_END_DECLS

