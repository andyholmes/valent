// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define VALENT_TYPE_MENU_STACK (valent_menu_stack_get_type())

G_DECLARE_FINAL_TYPE (ValentMenuStack, valent_menu_stack, VALENT, MENU_STACK, GtkWidget)

void              valent_menu_stack_bind_model (ValentMenuStack *self,
                                                GMenuModel      *model);
ValentMenuStack * valent_menu_stack_new        (void);

G_END_DECLS
