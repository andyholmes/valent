// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include "config.h"

#include <libpeas.h>
#include <valent.h>

#include "valent-mpris-adapter.h"
#include "valent-mpris-plugin.h"


_VALENT_EXTERN void
valent_mpris_plugin_register_types (PeasObjectModule *module)
{
  peas_object_module_register_extension_type (module,
                                              VALENT_TYPE_DEVICE_PLUGIN,
                                              VALENT_TYPE_MPRIS_PLUGIN);
  peas_object_module_register_extension_type (module,
                                              VALENT_TYPE_MEDIA_ADAPTER,
                                              VALENT_TYPE_MPRIS_ADAPTER);
}

