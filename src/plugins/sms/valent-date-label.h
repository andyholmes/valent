// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define VALENT_TYPE_DATE_LABEL (valent_date_label_get_type())

G_DECLARE_FINAL_TYPE (ValentDateLabel, valent_date_label, VALENT, DATE_LABEL, GtkWidget)

GtkWidget    * valent_date_label_new      (int64_t          date);

int64_t        valent_date_label_get_date (ValentDateLabel *label);
void           valent_date_label_set_date (ValentDateLabel *label,
                                           int64_t          date);
unsigned int   valent_date_label_get_mode (ValentDateLabel *label);
void           valent_date_label_set_mode (ValentDateLabel *label,
                                           unsigned int     mode);
void           valent_date_label_update   (ValentDateLabel *label);

char         * valent_date_label_long     (int64_t          date);
char         * valent_date_label_short    (int64_t          date);

G_END_DECLS
