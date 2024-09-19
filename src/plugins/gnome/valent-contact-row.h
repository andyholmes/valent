// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <gtk/gtk.h>
#include <valent.h>

G_BEGIN_DECLS

#define VALENT_TYPE_CONTACT_ROW (valent_contact_row_get_type())

G_DECLARE_FINAL_TYPE (ValentContactRow, valent_contact_row, VALENT, CONTACT_ROW, GtkListBoxRow)

EContact   * valent_contact_row_get_contact        (ValentContactRow *row);
void         valent_contact_row_set_contact        (ValentContactRow *row,
                                                    EContact         *contact);
const char * valent_contact_row_get_contact_medium (ValentContactRow *row);

void         valent_list_add_contact               (GtkListBox       *list,
                                                    EContact         *contact);
void         valent_contact_row_header_func        (GtkListBoxRow    *row,
                                                    GtkListBoxRow    *before,
                                                    gpointer          user_data);
int          valent_contact_row_sort_func          (GtkListBoxRow    *row1,
                                                    GtkListBoxRow    *row2,
                                                    gpointer          user_data);

G_END_DECLS
