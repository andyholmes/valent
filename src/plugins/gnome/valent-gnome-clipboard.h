// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <valent.h>

G_BEGIN_DECLS

#define VALENT_TYPE_GNOME_CLIPBOARD (valent_gnome_clipboard_get_type ())

G_DECLARE_FINAL_TYPE (ValentGnomeClipboard, valent_gnome_clipboard, VALENT, GNOME_CLIPBOARD, ValentClipboardAdapter)

G_END_DECLS

