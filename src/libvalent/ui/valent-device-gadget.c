// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-device-gadget"

#include "config.h"

#include <gtk/gtk.h>
#include <libvalent-core.h>

#include "valent-device-gadget.h"


/**
 * ValentDeviceGadget:
 *
 * An interface for small device widgets.
 *
 * #ValentDeviceGadget is an interface for plugins that want a small widget to
 * display controls, such as a battery level icon.
 *
 * Since: 1.0
 */

G_DEFINE_INTERFACE (ValentDeviceGadget, valent_device_gadget, GTK_TYPE_WIDGET)

/**
 * ValentDeviceGadgetInterface:
 *
 * The virtual function table for #ValentDeviceGadget.
 *
 * Since: 1.0
 */

static void
valent_device_gadget_default_init (ValentDeviceGadgetInterface *iface)
{
  /**
   * ValentDeviceGadget:device:
   *
   * The [class@Valent.Device] this gadget is for.
   *
   * Since: 1.0
   */
  g_object_interface_install_property (iface,
                                       g_param_spec_object ("device", NULL, NULL,
                                                            G_TYPE_OBJECT,
                                                            (G_PARAM_READWRITE |
                                                             G_PARAM_CONSTRUCT_ONLY |
                                                             G_PARAM_EXPLICIT_NOTIFY |
                                                             G_PARAM_STATIC_STRINGS)));
}

