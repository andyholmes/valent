// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <valent.h>

G_BEGIN_DECLS

#define VALENT_TYPE_XDP_CLIPBOARD (valent_xdp_clipboard_get_type ())

G_DECLARE_FINAL_TYPE (ValentXdpClipboard, valent_xdp_clipboard, VALENT, XDP_CLIPBOARD, ValentClipboardAdapter)

G_END_DECLS

