// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include "config.h"

#include <libpeas/peas.h>
#include <valent.h>

#include "valent-connectivity_report-gadget.h"
#include "valent-connectivity_report-plugin.h"
#include "valent-connectivity_report-preferences.h"


G_MODULE_EXPORT void
valent_connectivity_report_plugin_register_types (PeasObjectModule *module)
{
  peas_object_module_register_extension_type (module,
                                              VALENT_TYPE_DEVICE_PLUGIN,
                                              VALENT_TYPE_CONNECTIVITY_REPORT_PLUGIN);
  peas_object_module_register_extension_type (module,
                                              VALENT_TYPE_DEVICE_GADGET,
                                              VALENT_TYPE_CONNECTIVITY_REPORT_GADGET);
  peas_object_module_register_extension_type (module,
                                              VALENT_TYPE_DEVICE_PREFERENCES_PAGE,
                                              VALENT_TYPE_CONNECTIVITY_REPORT_PREFERENCES);
}
