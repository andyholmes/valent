// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define VALENT_TYPE_CONNECT_DIALOG (valent_connect_dialog_get_type())

G_DECLARE_FINAL_TYPE (ValentConnectDialog, valent_connect_dialog, VALENT, CONNECT_DIALOG, GtkDialog)

GtkDialog * valent_connect_dialog_new (void);

G_END_DECLS
