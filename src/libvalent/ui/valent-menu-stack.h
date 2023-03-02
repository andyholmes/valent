// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define VALENT_TYPE_MENU_STACK (valent_menu_stack_get_type())

G_DECLARE_FINAL_TYPE (ValentMenuStack, valent_menu_stack, VALENT, MENU_STACK, GtkWidget)

GtkWidget  * valent_menu_stack_new            (GMenuModel      *menu_model);
GMenuModel * valent_menu_stack_get_menu_model (ValentMenuStack *stack);
void         valent_menu_stack_set_menu_model (ValentMenuStack *stack,
                                               GMenuModel      *menu_model);

G_END_DECLS
