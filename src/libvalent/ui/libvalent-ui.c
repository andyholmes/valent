// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include "config.h"

#include <gio/gio.h>
#include <gtk/gtk.h>
#include <libpeas/peas.h>
#include <libvalent-core.h>

#include "valent-ui-manager.h"


_VALENT_EXTERN void
libvalent_ui_register_types (PeasObjectModule *module)
{
  if (gtk_is_initialized ())
    {
      peas_object_module_register_extension_type (module,
                                                  VALENT_TYPE_APPLICATION_PLUGIN,
                                                  VALENT_TYPE_UI_MANAGER);
    }
}

