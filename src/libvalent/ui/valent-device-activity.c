// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-device-activity"

#include "config.h"

#include <glib-object.h>

#include "valent-device-activity.h"


/**
 * SECTION:valent-device-activity
 * @short_description: Interface for device activity widgets
 * @title: ValentDeviceActivity
 * @stability: Unstable
 *
 * The #ValentDeviceActivity interface is typically implemented by device
 * plugins that need a large widget to display controls.
 *
 * A consumer of #ValentDeviceActivity widgets should typically present these
 * as pages in a #GtkStack or the content of a #GtkDialog.
 */

G_DEFINE_INTERFACE (ValentDeviceActivity, valent_device_activity, G_TYPE_OBJECT)

/**
 * ValentDeviceActivityInterface:
 *
 * The virtual function table for #ValentDeviceActivity.
 */

static void
valent_device_activity_default_init (ValentDeviceActivityInterface *iface)
{
  /**
   * ValentDeviceActivity:extension:
   *
   * The #ValentExtension this configures.
   */
  g_object_interface_install_property (iface,
                                       g_param_spec_object ("device",
                                                            "Device",
                                                            "Device",
                                                            G_TYPE_OBJECT,
                                                            (G_PARAM_READWRITE |
                                                             G_PARAM_CONSTRUCT_ONLY |
                                                             G_PARAM_EXPLICIT_NOTIFY |
                                                             G_PARAM_STATIC_STRINGS)));
}

