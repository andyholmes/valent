// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include "config.h"

#include <gio/gio.h>
#include <libpeas/peas.h>
#include <libvalent-mixer.h>

#include "valent-pa-mixer.h"


G_MODULE_EXPORT void
valent_pulseaudio_plugin_register_types (PeasObjectModule *module)
{
  peas_object_module_register_extension_type (module,
                                              VALENT_TYPE_MIXER_ADAPTER,
                                              VALENT_TYPE_PA_MIXER);
}
