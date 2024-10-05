// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <gtk/gtk.h>
#include <libebook-contacts/libebook-contacts.h>
#include <valent.h>

G_BEGIN_DECLS

#define VALENT_TYPE_CONTACT_ROW (valent_contact_row_get_type())

G_DECLARE_FINAL_TYPE (ValentContactRow, valent_contact_row, VALENT, CONTACT_ROW, GtkListBoxRow)

EContact   * valent_contact_row_get_contact        (ValentContactRow *row);
void         valent_contact_row_set_contact        (ValentContactRow *row,
                                                    EContact         *contact);

G_END_DECLS
