// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_UI_INSIDE) && !defined (VALENT_UI_COMPILATION)
# error "Only <libvalent-ui.h> can be included directly."
#endif

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define VALENT_TYPE_APPLICATION (valent_application_get_type())

G_DECLARE_FINAL_TYPE (ValentApplication, valent_application, VALENT, APPLICATION, GtkApplication)

ValentApplication * _valent_application_new (void);

G_END_DECLS
