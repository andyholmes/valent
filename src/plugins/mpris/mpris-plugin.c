// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include "config.h"

#include <libpeas/peas.h>
#include <libvalent-core.h>
#include <libvalent-media.h>

#include "valent-mpris-plugin.h"
#include "valent-mpris-player-provider.h"


G_MODULE_EXPORT void
valent_mpris_plugin_register_types (PeasObjectModule *module)
{
  peas_object_module_register_extension_type (module,
                                              VALENT_TYPE_DEVICE_PLUGIN,
                                              VALENT_TYPE_MPRIS_PLUGIN);
  peas_object_module_register_extension_type (module,
                                              VALENT_TYPE_MEDIA_PLAYER_PROVIDER,
                                              VALENT_TYPE_MPRIS_PLAYER_PROVIDER);
}

