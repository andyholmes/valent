// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <valent.h>

G_BEGIN_DECLS

#define VALENT_TYPE_CLIPBOARD_PLUGIN (valent_clipboard_plugin_get_type())

G_DECLARE_FINAL_TYPE (ValentClipboardPlugin, valent_clipboard_plugin, VALENT, CLIPBOARD_PLUGIN, ValentDevicePlugin)

G_END_DECLS

