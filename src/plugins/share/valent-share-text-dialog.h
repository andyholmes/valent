// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <adwaita.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define VALENT_TYPE_SHARE_TEXT_DIALOG (valent_share_text_dialog_get_type())

G_DECLARE_FINAL_TYPE (ValentShareTextDialog, valent_share_text_dialog, VALENT, SHARE_TEXT_DIALOG, AdwMessageDialog)

const char * valent_share_text_dialog_get_text (ValentShareTextDialog *dialog);
void         valent_share_text_dialog_set_text (ValentShareTextDialog *dialog,
                                                const char            *text);

G_END_DECLS

