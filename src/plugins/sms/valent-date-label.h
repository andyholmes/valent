// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define VALENT_TYPE_DATE_LABEL (valent_date_label_get_type())

G_DECLARE_FINAL_TYPE (ValentDateLabel, valent_date_label, VALENT, DATE_LABEL, GtkWidget)

GtkWidget * valent_date_label_new      (gint64           date);

gint64      valent_date_label_get_date (ValentDateLabel *label);
void        valent_date_label_set_date (ValentDateLabel *label,
                                        gint64           date);
guint       valent_date_label_get_mode (ValentDateLabel *label);
void        valent_date_label_set_mode (ValentDateLabel *label,
                                        guint            mode);
void        valent_date_label_update   (ValentDateLabel *label);


char      * valent_date_label_long     (gint64           date);
char      * valent_date_label_short    (gint64           date);

G_END_DECLS
