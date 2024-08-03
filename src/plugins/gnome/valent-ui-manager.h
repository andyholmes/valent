// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <valent.h>

G_BEGIN_DECLS

#define VALENT_TYPE_UI_MANAGER (valent_ui_manager_get_type())

G_DECLARE_FINAL_TYPE (ValentUIManager, valent_ui_manager, VALENT, UI_MANAGER, ValentApplicationPlugin)

G_END_DECLS

