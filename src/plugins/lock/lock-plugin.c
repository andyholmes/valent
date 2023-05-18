// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include "config.h"

#include <libpeas/peas.h>
#include <valent.h>

#include "valent-lock-gadget.h"
#include "valent-lock-plugin.h"


_VALENT_EXTERN void
valent_lock_plugin_register_types (PeasObjectModule *module)
{
  peas_object_module_register_extension_type (module,
                                              VALENT_TYPE_DEVICE_PLUGIN,
                                              VALENT_TYPE_LOCK_PLUGIN);
  peas_object_module_register_extension_type (module,
                                              VALENT_TYPE_DEVICE_GADGET,
                                              VALENT_TYPE_LOCK_GADGET);
}

