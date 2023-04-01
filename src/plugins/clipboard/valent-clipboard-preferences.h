// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <valent.h>

G_BEGIN_DECLS

#define VALENT_TYPE_CLIPBOARD_PREFERENCES (valent_clipboard_preferences_get_type())

G_DECLARE_FINAL_TYPE (ValentClipboardPreferences, valent_clipboard_preferences, VALENT, CLIPBOARD_PREFERENCES, ValentDevicePreferencesGroup)

G_END_DECLS
