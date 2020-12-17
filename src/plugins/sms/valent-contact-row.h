// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <gtk/gtk.h>
#include <libvalent-contacts.h>

G_BEGIN_DECLS

#define VALENT_TYPE_CONTACT_ROW (valent_contact_row_get_type())

G_DECLARE_FINAL_TYPE (ValentContactRow, valent_contact_row, VALENT, CONTACT_ROW, GtkListBoxRow)

void         valent_list_add_contact            (GtkListBox             *list,
                                                 EContact               *contact);

void         valent_contact_row_header_func     (GtkListBoxRow          *row,
                                                 GtkListBoxRow          *before,
                                                 gpointer                user_data);

GtkWidget  * valent_contact_row_new             (EContact               *contact);

void         valent_contact_row_set_compact     (ValentContactRow       *row,
                                                 gboolean                compact);
EContact   * valent_contact_row_get_contact     (ValentContactRow       *row);
void         valent_contact_row_set_contact     (ValentContactRow       *row,
                                                 EContact               *contact);
const char * valent_contact_row_get_name        (ValentContactRow       *row);
void         valent_contact_row_set_name        (ValentContactRow       *row,
                                                 const char             *name);
const char * valent_contact_row_get_number      (ValentContactRow       *row);
void         valent_contact_row_set_number      (ValentContactRow       *row,
                                                 const char             *number);
void         valent_contact_row_set_number_full (ValentContactRow       *row,
                                                 const char             *number,
                                                 ValentPhoneNumberFlags  type);

G_END_DECLS
