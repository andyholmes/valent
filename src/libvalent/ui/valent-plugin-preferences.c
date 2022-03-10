// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-plugin-preferences"

#include "config.h"

#include <gio/gio.h>
#include <gtk/gtk.h>
#include <adwaita.h>

#include "valent-plugin-preferences.h"


/**
 * ValentPluginPreferences:
 *
 * An interface for plugin preferences.
 *
 * #ValentPluginPreferences is an interface for plugins that want to expose
 * configuration options for an extension.
 */

G_DEFINE_INTERFACE (ValentPluginPreferences, valent_plugin_preferences, GTK_TYPE_WIDGET)

/**
 * ValentPluginPreferencesInterface:
 *
 * The virtual function table for #ValentPluginPreferences.
 */

static void
valent_plugin_preferences_default_init (ValentPluginPreferencesInterface *iface)
{
  /**
   * ValentPluginPreferences:plugin-context:
   *
   * The context for the plugin this configures.
   */
  g_object_interface_install_property (iface,
                                       g_param_spec_string ("plugin-context",
                                                            "Plugin Context",
                                                            "Plugin Context",
                                                            NULL,
                                                            (G_PARAM_READWRITE |
                                                             G_PARAM_CONSTRUCT_ONLY |
                                                             G_PARAM_EXPLICIT_NOTIFY |
                                                             G_PARAM_STATIC_STRINGS)));
}

/**
 * valent_plugin_preferences_row_sort:
 * @row1: a #GtkListBoxRow
 * @row2: a #GtkListBoxRow
 * @user_data: user defined data
 *
 * Compare two #AdwPreferencesRow instances by title to determine which should
 * be first. If either row is not an instance of #AdwPreferencesRow the rows
 * will be considered equal.
 *
 * Returns: < 0 if @row1 should be before @row2 , 0 if they are equal
 *     and > 0 otherwise
 */
int
valent_plugin_preferences_row_sort (GtkListBoxRow *row1,
                                    GtkListBoxRow *row2,
                                    gpointer       user_data)
{
  if G_UNLIKELY (!ADW_IS_PREFERENCES_ROW (row1) ||
                 !ADW_IS_PREFERENCES_ROW (row2))
    return 0;

  return g_utf8_collate (adw_preferences_row_get_title ((AdwPreferencesRow *)row1),
                         adw_preferences_row_get_title ((AdwPreferencesRow *)row2));
}

