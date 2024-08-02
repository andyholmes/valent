// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <valent.h>

G_BEGIN_DECLS

#define VALENT_TYPE_SHARE_DIALOG_ROW (valent_share_dialog_row_get_type())

G_DECLARE_FINAL_TYPE (ValentShareDialogRow, valent_share_dialog_row, VALENT, SHARE_DIALOG_ROW, GtkListBoxRow)

ValentDevice * valent_share_dialog_row_get_device         (ValentShareDialogRow *row);
gboolean       valent_share_dialog_row_get_selected       (ValentShareDialogRow *row);
void           valent_share_dialog_row_set_selected       (ValentShareDialogRow *row,
                                                           gboolean              selected);
gboolean       valent_share_dialog_row_get_selection_mode (ValentShareDialogRow *row);
void           valent_share_dialog_row_set_selection_mode (ValentShareDialogRow *row,
                                                           gboolean              selection_mode);

G_END_DECLS
