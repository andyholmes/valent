// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <valent.h>

G_BEGIN_DECLS

#define VALENT_TYPE_MOCK_CLIPBOARD_ADAPTER (valent_mock_clipboard_adapter_get_type ())

G_DECLARE_FINAL_TYPE (ValentMockClipboardAdapter, valent_mock_clipboard_adapter, VALENT, MOCK_CLIPBOARD_ADAPTER, ValentClipboardAdapter)

G_END_DECLS

