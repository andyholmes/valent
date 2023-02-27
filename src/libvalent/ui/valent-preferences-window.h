// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <adwaita.h>
#include <libvalent-core.h>

G_BEGIN_DECLS

#define VALENT_TYPE_PREFERENCES_WINDOW (valent_preferences_window_get_type())

_VALENT_EXTERN
G_DECLARE_FINAL_TYPE (ValentPreferencesWindow, valent_preferences_window, VALENT, PREFERENCES_WINDOW, AdwPreferencesWindow)

gboolean   valent_preferences_window_modify (AdwPreferencesWindow *window);

G_END_DECLS
