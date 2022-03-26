// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include "config.h"

#include <gtk/gtk.h>
#include <libpeas/peas.h>
#include <libvalent-input.h>

#include "valent-xdp-background.h"
#include "valent-xdp-input.h"


G_MODULE_EXPORT void
valent_xdp_plugin_register_types (PeasObjectModule *module)
{
  /* Ensure this is GUI instance before registering */
  if (!gtk_init_check ())
    return;

  peas_object_module_register_extension_type (module,
                                              VALENT_TYPE_INPUT_ADAPTER,
                                              VALENT_TYPE_XDP_INPUT);

  /* Ensure this is a Flatpak instance before registering */
  if (!valent_in_flatpak ())
    return;

  peas_object_module_register_extension_type (module,
                                              VALENT_TYPE_APPLICATION_PLUGIN,
                                              VALENT_TYPE_XDP_BACKGROUND);
}

