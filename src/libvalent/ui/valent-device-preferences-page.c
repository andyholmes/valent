// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-device-preferences"

#include "config.h"

#include <gio/gio.h>
#include <gtk/gtk.h>
#include <adwaita.h>

#include "valent-device-preferences-page.h"


/**
 * ValentDevicePreferencesPage:
 *
 * An interface for device plugin preferences.
 *
 * #ValentDevicePreferencesPage is an interface for [class@Valent.DevicePlugin]
 * implementations that want to provide a preferences page.
 *
 * Since: 1.0
 */

G_DEFINE_INTERFACE (ValentDevicePreferencesPage, valent_device_preferences_page, ADW_TYPE_PREFERENCES_PAGE)

/**
 * ValentDevicePreferencesPageInterface:
 *
 * The virtual function table for #ValentDevicePreferencesPage.
 */

static void
valent_device_preferences_page_default_init (ValentDevicePreferencesPageInterface *iface)
{
  /**
   * ValentDevicePreferencesPage:device-id:
   *
   * The ID of the [class@Valent.Device] the plugin is bound to.
   *
   * Since: 1.0
   */
  g_object_interface_install_property (iface,
                                       g_param_spec_string ("device-id", NULL, NULL,
                                                            NULL,
                                                            (G_PARAM_READWRITE |
                                                             G_PARAM_CONSTRUCT_ONLY |
                                                             G_PARAM_EXPLICIT_NOTIFY |
                                                             G_PARAM_STATIC_STRINGS)));

  /**
   * ValentDevicePreferencesPage:plugin-info:
   *
   * The [struct@Peas.PluginInfo] describing the plugin this page configures.
   *
   * Since: 1.0
   */
  g_object_interface_install_property (iface,
                                       g_param_spec_boxed ("plugin-info", NULL, NULL,
                                                           PEAS_TYPE_PLUGIN_INFO,
                                                           (G_PARAM_READWRITE |
                                                            G_PARAM_CONSTRUCT_ONLY |
                                                            G_PARAM_EXPLICIT_NOTIFY |
                                                            G_PARAM_STATIC_STRINGS)));
}

