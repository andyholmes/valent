// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define VALENT_TYPE_DATE_FORMAT (valent_date_format_get_type())

typedef enum
{
  VALENT_DATE_FORMAT_ADAPTIVE,
  VALENT_DATE_FORMAT_ADAPTIVE_SHORT,
  VALENT_DATE_FORMAT_TIME,
} ValentDateFormat;

#define VALENT_TYPE_DATE_LABEL (valent_date_label_get_type())

G_DECLARE_FINAL_TYPE (ValentDateLabel, valent_date_label, VALENT, DATE_LABEL, GtkWidget)

int64_t            valent_date_label_get_date (ValentDateLabel  *label);
void               valent_date_label_set_date (ValentDateLabel  *label,
                                               int64_t           date);
ValentDateFormat   valent_date_label_get_mode (ValentDateLabel  *label);
void               valent_date_label_set_mode (ValentDateLabel  *label,
                                               ValentDateFormat  mode);

G_END_DECLS
