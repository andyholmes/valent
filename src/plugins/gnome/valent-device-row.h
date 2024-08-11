// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <gtk/gtk.h>
#include <valent.h>

G_BEGIN_DECLS

#define VALENT_TYPE_DEVICE_ROW (valent_device_row_get_type())

G_DECLARE_FINAL_TYPE (ValentDeviceRow, valent_device_row, VALENT, DEVICE_ROW, GtkListBoxRow)

ValentDevice * valent_device_row_get_device         (ValentDeviceRow *row);
gboolean       valent_device_row_get_selected       (ValentDeviceRow *row);
void           valent_device_row_set_selected       (ValentDeviceRow *row,
                                                     gboolean         selected);
gboolean       valent_device_row_get_selection_mode (ValentDeviceRow *row);
void           valent_device_row_set_selection_mode (ValentDeviceRow *row,
                                                     gboolean         selection_mode);

G_END_DECLS
