// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include "config.h"

#include <libpeas/peas.h>
#include <valent.h>

#include "valent-pa-mixer.h"


_VALENT_EXTERN void
valent_pulseaudio_plugin_register_types (PeasObjectModule *module)
{
  peas_object_module_register_extension_type (module,
                                              VALENT_TYPE_MIXER_ADAPTER,
                                              VALENT_TYPE_PA_MIXER);
}
