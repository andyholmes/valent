// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include "valent-mousepad-plugin.h"

#include <adwaita.h>
#include <gtk/gtk.h>
#include <json-glib/json-glib.h>

G_BEGIN_DECLS

#define VALENT_TYPE_INPUT_DIALOG (valent_input_dialog_get_type())

G_DECLARE_FINAL_TYPE (ValentInputDialog, valent_input_dialog, VALENT, INPUT_DIALOG, AdwWindow)

ValentInputDialog * valent_input_dialog_new          (ValentMousepadPlugin *plugin);

void                valent_input_dialog_echo_key     (ValentInputDialog  *dialog,
                                                      const char         *key,
                                                      GdkModifierType     mask);
void                valent_input_dialog_echo_special (ValentInputDialog  *dialog,
                                                      unsigned int        keyval,
                                                      GdkModifierType     mask);

G_END_DECLS
