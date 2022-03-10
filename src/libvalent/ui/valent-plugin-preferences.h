// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_UI_INSIDE) && !defined (VALENT_UI_COMPILATION)
# error "Only <libvalent-ui.h> can be included directly."
#endif

#include <gtk/gtk.h>
#include <libvalent-core.h>

G_BEGIN_DECLS

#define VALENT_TYPE_PLUGIN_PREFERENCES (valent_plugin_preferences_get_type ())

VALENT_AVAILABLE_IN_1_0
G_DECLARE_INTERFACE (ValentPluginPreferences, valent_plugin_preferences, VALENT, PLUGIN_PREFERENCES, GtkWidget)

struct _ValentPluginPreferencesInterface
{
  GTypeInterface  g_iface;
};

/* Utility Functions */
VALENT_AVAILABLE_IN_1_0
int   valent_plugin_preferences_row_sort (GtkListBoxRow *row1,
                                          GtkListBoxRow *row2,
                                          gpointer       user_data);

G_END_DECLS

