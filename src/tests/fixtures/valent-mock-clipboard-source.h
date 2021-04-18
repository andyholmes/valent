// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_TEST_INSIDE) && !defined (VALENT_TEST_COMPILATION)
# error "Only <libvalent-test.h> can be included directly."
#endif

#include <libvalent-core.h>
#include <libvalent-clipboard.h>

G_BEGIN_DECLS

#define VALENT_TYPE_MOCK_CLIPBOARD_SOURCE (valent_mock_clipboard_source_get_type ())

G_DECLARE_FINAL_TYPE (ValentMockClipboardSource, valent_mock_clipboard_source, VALENT, MOCK_CLIPBOARD_SOURCE, ValentClipboardSource)

G_END_DECLS

