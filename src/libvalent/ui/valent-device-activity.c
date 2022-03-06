// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-device-activity"

#include "config.h"

#include <gtk/gtk.h>
#include <libvalent-core.h>

#include "valent-device-activity.h"


/**
 * ValentDeviceActivity:
 *
 * An interface for large device widgets.
 *
 * #ValentDeviceActivity is an interface for plugins that want a large widget to
 * display controls, such as a media player remote.
 */

G_DEFINE_INTERFACE (ValentDeviceActivity, valent_device_activity, GTK_TYPE_WIDGET)

/**
 * ValentDeviceActivityInterface:
 *
 * The virtual function table for #ValentDeviceActivity.
 */

static void
valent_device_activity_default_init (ValentDeviceActivityInterface *iface)
{
  /**
   * ValentDeviceActivity:device:
   *
   * The [class@Valent.Device] this activity is for.
   */
  g_object_interface_install_property (iface,
                                       g_param_spec_object ("device",
                                                            "Device",
                                                            "The device this activity is for",
                                                            G_TYPE_OBJECT,
                                                            (G_PARAM_READWRITE |
                                                             G_PARAM_CONSTRUCT_ONLY |
                                                             G_PARAM_EXPLICIT_NOTIFY |
                                                             G_PARAM_STATIC_STRINGS)));
}

