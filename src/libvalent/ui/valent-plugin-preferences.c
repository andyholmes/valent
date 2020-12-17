// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-ui"

#include "config.h"

#include <gio/gio.h>
#include <gtk/gtk.h>
#include <adwaita.h>

#include "valent-plugin-preferences.h"


/**
 * SECTION:valent-plugin-preferences
 * @short_description: Interface for extension preferences
 * @title: ValentPluginPreferences
 * @stability: Unstable
 *
 * The #ValentPluginPreferences interface should be implemented by extensions
 * that want to provide a GUI for configuration. How the interface is used and
 * presented is dependant on the extension type, as the interface may be used to
 * configure a device plugin, a channel provider or session component.
 */

G_DEFINE_INTERFACE (ValentPluginPreferences, valent_plugin_preferences, G_TYPE_OBJECT)

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
 * valent_plugin_preferences_create_settings:
 * @context: a context string
 * @module: a module name
 *
 * Create a new #GSettings object for this settings, or %NULL if the schema can
 * not be found.
 *
 * Returns: (nullable) (transfer full): a #GSettings
 */
GSettings *
valent_plugin_preferences_create_settings (const char *context,
                                           const char *module)
{
  GSettingsSchemaSource *source;
  GSettingsSchema *schema;
  g_autofree char *path = NULL;
  g_autofree char *schema_id = NULL;

  g_return_val_if_fail (context != NULL, NULL);
  g_return_val_if_fail (module != NULL, NULL);

  path = g_strdup_printf ("/ca/andyholmes/valent/device/%s/%s/", context, module);
  schema_id = g_strdup_printf ("ca.andyholmes.valent.%s", module);

  source = g_settings_schema_source_get_default ();
  schema = g_settings_schema_source_lookup (source, schema_id, TRUE);

  if (schema)
    return g_settings_new_full (schema, NULL, path);

  return NULL;
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

