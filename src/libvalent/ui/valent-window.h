// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define VALENT_TYPE_WINDOW (valent_window_get_type())

G_DECLARE_FINAL_TYPE (ValentWindow, valent_window, VALENT, WINDOW, AdwApplicationWindow)

G_END_DECLS
