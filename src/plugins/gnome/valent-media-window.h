// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define VALENT_TYPE_MEDIA_WINDOW (valent_media_window_get_type())

G_DECLARE_FINAL_TYPE (ValentMediaWindow, valent_media_window, VALENT, MEDIA_WINDOW, AdwWindow)

G_END_DECLS

