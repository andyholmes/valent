// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <adwaita.h>
#include <valent.h>

G_BEGIN_DECLS

#define VALENT_TYPE_MESSAGES_WINDOW (valent_messages_window_get_type())

G_DECLARE_FINAL_TYPE (ValentMessagesWindow, valent_messages_window, VALENT, MESSAGES_WINDOW, AdwApplicationWindow)

G_END_DECLS
