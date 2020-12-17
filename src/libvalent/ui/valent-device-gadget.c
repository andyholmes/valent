// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-device-gadget"

#include "config.h"

#include <glib-object.h>

#include "valent-device-gadget.h"


/**
 * SECTION:valent-device-gadget
 * @short_description: Interface for device plugin widgets
 * @title: ValentDeviceGadget
 * @stability: Unstable
 *
 * The #ValentDeviceGadget interface is typically implemented by device
 * plugins that need a small widget to display controls.
 *
 * A consumer of #ValentDeviceGadget widgets should typically present these
 * as items in an action bar or toolbar.
 */

G_DEFINE_INTERFACE (ValentDeviceGadget, valent_device_gadget, G_TYPE_OBJECT)

/**
 * ValentDeviceGadgetInterface:
 *
 * The virtual function table for #ValentDeviceGadget.
 */

static void
valent_device_gadget_default_init (ValentDeviceGadgetInterface *iface)
{
  /**
   * ValentDeviceGadget:extension:
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

