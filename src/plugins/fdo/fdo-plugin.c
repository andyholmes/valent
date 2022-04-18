// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include "config.h"

#include <gtk/gtk.h>
#include <libpeas/peas.h>
#include <libvalent-notifications.h>
#include <libvalent-session.h>

#include "valent-fdo-notifications.h"
#include "valent-fdo-session.h"


G_MODULE_EXPORT void
valent_fdo_plugin_register_types (PeasObjectModule *module)
{
  /* Although notifications typically only make sense in a graphical
   * environment, it is standard interface that one could implement for devices
   * without a graphical display (ie. home automation device). */
  peas_object_module_register_extension_type (module,
                                              VALENT_TYPE_NOTIFICATION_SOURCE,
                                              VALENT_TYPE_FDO_NOTIFICATIONS);

  peas_object_module_register_extension_type (module,
                                              VALENT_TYPE_SESSION_ADAPTER,
                                              VALENT_TYPE_FDO_SESSION);
}
