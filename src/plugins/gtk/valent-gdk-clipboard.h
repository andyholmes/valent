// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <libvalent-core.h>
#include <libvalent-clipboard.h>

G_BEGIN_DECLS

#define VALENT_TYPE_GDK_CLIPBOARD (valent_gdk_clipboard_get_type ())

G_DECLARE_FINAL_TYPE (ValentGdkClipboard, valent_gdk_clipboard, VALENT, GDK_CLIPBOARD, ValentClipboardSource)

G_END_DECLS

