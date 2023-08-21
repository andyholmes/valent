// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include "config.h"

#include <libpeas.h>
#include <valent.h>

#include "valent-battery-gadget.h"
#include "valent-battery-plugin.h"
#include "valent-battery-preferences.h"


_VALENT_EXTERN void
valent_battery_plugin_register_types (PeasObjectModule *module)
{
  peas_object_module_register_extension_type (module,
                                              VALENT_TYPE_DEVICE_PLUGIN,
                                              VALENT_TYPE_BATTERY_PLUGIN);
  peas_object_module_register_extension_type (module,
                                              VALENT_TYPE_DEVICE_GADGET,
                                              VALENT_TYPE_BATTERY_GADGET);
  peas_object_module_register_extension_type (module,
                                              VALENT_TYPE_DEVICE_PREFERENCES_GROUP,
                                              VALENT_TYPE_BATTERY_PREFERENCES);
}
