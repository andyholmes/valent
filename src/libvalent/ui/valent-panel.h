// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_UI_INSIDE) && !defined (VALENT_UI_COMPILATION)
# error "Only <libvalent-ui.h> can be included directly."
#endif

#include <gtk/gtk.h>
#include <libvalent-core.h>

G_BEGIN_DECLS

#define VALENT_TYPE_PANEL (valent_panel_get_type())

VALENT_AVAILABLE_IN_1_0
G_DECLARE_DERIVABLE_TYPE (ValentPanel, valent_panel, VALENT, PANEL, GtkWidget)

struct _ValentPanelClass
{
  GtkWidgetClass   parent_class;
};

VALENT_AVAILABLE_IN_1_0
GtkWidget  * valent_panel_new           (void);
VALENT_AVAILABLE_IN_1_0
void         valent_panel_append        (ValentPanel *panel,
                                         GtkWidget   *child);
VALENT_AVAILABLE_IN_1_0
void         valent_panel_prepend       (ValentPanel *panel,
                                         GtkWidget   *child);
VALENT_AVAILABLE_IN_1_0
const char * valent_panel_get_icon_name (ValentPanel *panel);
VALENT_AVAILABLE_IN_1_0
void         valent_panel_set_icon_name (ValentPanel *panel,
                                         const char  *icon_name);
VALENT_AVAILABLE_IN_1_0
const char * valent_panel_get_title     (ValentPanel *panel);
VALENT_AVAILABLE_IN_1_0
void         valent_panel_set_title     (ValentPanel *panel,
                                         const char  *title);
VALENT_AVAILABLE_IN_1_0
GtkWidget  * valent_panel_get_header    (ValentPanel *panel);
VALENT_AVAILABLE_IN_1_0
void         valent_panel_set_header    (ValentPanel *panel,
                                         GtkWidget   *child);
VALENT_AVAILABLE_IN_1_0
GtkWidget  * valent_panel_get_footer    (ValentPanel *panel);
VALENT_AVAILABLE_IN_1_0
void         valent_panel_set_footer    (ValentPanel *panel,
                                         GtkWidget   *child);

G_END_DECLS
