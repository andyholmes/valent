// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include "config.h"

#include <libpeas.h>
#include <valent.h>

#include "valent-connectivity_report-plugin.h"
#include "valent-connectivity_report-preferences.h"


_VALENT_EXTERN void
valent_connectivity_report_plugin_register_types (PeasObjectModule *module)
{
  peas_object_module_register_extension_type (module,
                                              VALENT_TYPE_DEVICE_PLUGIN,
                                              VALENT_TYPE_CONNECTIVITY_REPORT_PLUGIN);
  peas_object_module_register_extension_type (module,
                                              VALENT_TYPE_DEVICE_PREFERENCES_GROUP,
                                              VALENT_TYPE_CONNECTIVITY_REPORT_PREFERENCES);
}
