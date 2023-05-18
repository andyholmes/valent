// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include "config.h"

#include <gio/gio.h>
#include <libpeas/peas.h>

#include "valent-device-manager.h"


_VALENT_EXTERN void
libvalent_device_register_types (PeasObjectModule *module)
{
  peas_object_module_register_extension_type (module,
                                              VALENT_TYPE_APPLICATION_PLUGIN,
                                              VALENT_TYPE_DEVICE_MANAGER);
}
