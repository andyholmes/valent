// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include "config.h"

#include <gio/gio.h>
#include <libpeas/peas.h>
#include <valent.h>

#include "valent-gnome-clipboard.h"
#include "valent-mutter-input.h"


_VALENT_EXTERN void
valent_gnome_plugin_register_types (PeasObjectModule *module)
{
  peas_object_module_register_extension_type (module,
                                              VALENT_TYPE_CLIPBOARD_ADAPTER,
                                              VALENT_TYPE_GNOME_CLIPBOARD);
  peas_object_module_register_extension_type (module,
                                              VALENT_TYPE_INPUT_ADAPTER,
                                              VALENT_TYPE_MUTTER_INPUT);
}
