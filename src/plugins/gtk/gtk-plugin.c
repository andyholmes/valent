// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include "config.h"

#include <gtk/gtk.h>
#include <libpeas/peas.h>
#include <libvalent-clipboard.h>
#include <libvalent-notifications.h>
#include <libvalent-session.h>

#include "valent-gdk-clipboard.h"
#include "valent-gnome-session.h"
#include "valent-gtk-notifications.h"


G_MODULE_EXPORT void
valent_gtk_plugin_register_types (PeasObjectModule *module)
{
  /* Ensure this is GUI instance before registering */
  if (gtk_init_check () && GDK_IS_DISPLAY (gdk_display_get_default ()))
    {
      /* ValentClipboardSource */
      peas_object_module_register_extension_type (module,
                                                  VALENT_TYPE_CLIPBOARD_SOURCE,
                                                  VALENT_TYPE_GDK_CLIPBOARD);

      /* ValentNotificationSource */
      peas_object_module_register_extension_type (module,
                                                  VALENT_TYPE_NOTIFICATION_SOURCE,
                                                  VALENT_TYPE_GTK_NOTIFICATIONS);

      /* ValentSessionAdapter */
      peas_object_module_register_extension_type (module,
                                                  VALENT_TYPE_SESSION_ADAPTER,
                                                  VALENT_TYPE_GNOME_SESSION);
    }
}
