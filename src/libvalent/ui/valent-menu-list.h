// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define VALENT_TYPE_MENU_LIST (valent_menu_list_get_type())

G_DECLARE_FINAL_TYPE (ValentMenuList, valent_menu_list, VALENT, MENU_LIST, GtkWidget)

ValentMenuList * valent_menu_list_new            (GMenuModel     *model);
GMenuModel     * valent_menu_list_get_model      (ValentMenuList *self);
void             valent_menu_list_set_model      (ValentMenuList *self,
                                                  GMenuModel     *model);
ValentMenuList * valent_menu_list_get_submenu_of (ValentMenuList *self);
void             valent_menu_list_set_submenu_of (ValentMenuList *self,
                                                  ValentMenuList *parent);

G_END_DECLS
