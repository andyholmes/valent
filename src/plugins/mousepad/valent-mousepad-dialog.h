// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <adwaita.h>
#include <gtk/gtk.h>
#include <libvalent-core.h>

G_BEGIN_DECLS

#define VALENT_TYPE_MOUSEPAD_DIALOG (valent_mousepad_dialog_get_type())

G_DECLARE_FINAL_TYPE (ValentMousepadDialog, valent_mousepad_dialog, VALENT, MOUSEPAD_DIALOG, AdwWindow)

ValentMousepadDialog * valent_mousepad_dialog_new          (ValentDevice         *device);

void                   valent_mousepad_dialog_echo_key     (ValentMousepadDialog *dialog,
                                                            const char           *key,
                                                            GdkModifierType       mask);
void                   valent_mousepad_dialog_echo_special (ValentMousepadDialog *dialog,
                                                            unsigned int          keyval,
                                                            GdkModifierType       mask);

G_END_DECLS
