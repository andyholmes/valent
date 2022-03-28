// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-preferences-page"

#include "config.h"

#include <adwaita.h>
#include <libvalent-core.h>

#include "valent-preferences-page.h"


/**
 * ValentPreferencesPage:
 *
 * An interface for plugin preferences.
 *
 * #ValentPreferencesPage is an interface for plugins that want to provide a
 * preferences page. Unlike [iface@Valent.DevicePreferencesPage] the page should
 * configure all of the plugin's extension implementations, with the exception
 * of [class@Valent.DevicePlugin].
 *
 * Implementations of [class@Valent.DevicePlugin] should instead implement
 * [iface@Valent.DevicePreferencesPage], which will allow plugins to store
 * per-devices settings.
 */

G_DEFINE_INTERFACE (ValentPreferencesPage, valent_preferences_page, ADW_TYPE_PREFERENCES_PAGE)

/**
 * ValentPreferencesPageInterface:
 *
 * The virtual function table for #ValentPreferencesPage.
 */

static void
valent_preferences_page_default_init (ValentPreferencesPageInterface *iface)
{
  /**
   * ValentPreferencesPage:plugin-info:
   *
   * The [struct@Peas.PluginInfo] describing the plugin this page configures.
   */
  g_object_interface_install_property (iface,
                                       g_param_spec_boxed ("plugin-info",
                                                           "Plugin Info",
                                                           "The plugin info describing the plugin this page configures",
                                                           PEAS_TYPE_PLUGIN_INFO,
                                                           (G_PARAM_READWRITE |
                                                            G_PARAM_CONSTRUCT_ONLY |
                                                            G_PARAM_EXPLICIT_NOTIFY |
                                                            G_PARAM_STATIC_STRINGS)));
}

