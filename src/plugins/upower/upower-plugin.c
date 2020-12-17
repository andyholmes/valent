// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include "config.h"

#include <libpeas/peas.h>
#include <libvalent-power.h>

#include "valent-upower-device-provider.h"


G_MODULE_EXPORT void
valent_upower_plugin_register_types (PeasObjectModule *module)
{
  peas_object_module_register_extension_type (module,
                                              VALENT_TYPE_POWER_DEVICE_PROVIDER,
                                              VALENT_TYPE_UPOWER_DEVICE_PROVIDER);
}

