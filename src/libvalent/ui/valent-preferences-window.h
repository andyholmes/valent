// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define VALENT_TYPE_PREFERENCES_WINDOW (valent_preferences_window_get_type())

G_DECLARE_FINAL_TYPE (ValentPreferencesWindow, valent_preferences_window, VALENT, PREFERENCES_WINDOW, AdwPreferencesWindow)

G_END_DECLS
