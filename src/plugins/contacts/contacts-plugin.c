// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include "config.h"

#include <libpeas/peas.h>
#include <valent.h>

#include "valent-contacts-plugin.h"
#include "valent-contacts-preferences.h"


_VALENT_EXTERN void
valent_contacts_plugin_register_types (PeasObjectModule *module)
{
  peas_object_module_register_extension_type (module,
                                              VALENT_TYPE_DEVICE_PLUGIN,
                                              VALENT_TYPE_CONTACTS_PLUGIN);

  peas_object_module_register_extension_type (module,
                                              VALENT_TYPE_DEVICE_PREFERENCES_GROUP,
                                              VALENT_TYPE_CONTACTS_PREFERENCES);
}
