// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include "config.h"

#include <gtk/gtk.h>
#include <libpeas/peas.h>
#include <libvalent-clipboard.h>
#include <libvalent-notifications.h>

#include "valent-gdk-clipboard.h"
#include "valent-gtk-notifications.h"


G_MODULE_EXPORT void
valent_gtk_plugin_register_types (PeasObjectModule *module)
{
  /* These extensions inherently rely on GTK being initialized */
  if (gtk_is_initialized ())
    {
      peas_object_module_register_extension_type (module,
                                                  VALENT_TYPE_CLIPBOARD_ADAPTER,
                                                  VALENT_TYPE_GDK_CLIPBOARD);
      peas_object_module_register_extension_type (module,
                                                  VALENT_TYPE_NOTIFICATIONS_ADAPTER,
                                                  VALENT_TYPE_GTK_NOTIFICATIONS);
    }
}
