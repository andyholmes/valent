// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include "config.h"

#include <gtk/gtk.h>
#include <libpeas/peas.h>
#include <libportal/portal.h>
#include <valent.h>

#include "valent-xdp-background.h"
#include "valent-xdp-input.h"
#include "valent-xdp-session.h"


_VALENT_EXTERN void
valent_xdp_plugin_register_types (PeasObjectModule *module)
{
  /* This extension only makes sense in a graphical environment. */
  if (gtk_is_initialized ())
    {
      peas_object_module_register_extension_type (module,
                                                  VALENT_TYPE_INPUT_ADAPTER,
                                                  VALENT_TYPE_XDP_INPUT);
    }

  /* These extensions only makes sense in a sandbox environment. */
  if (xdp_portal_running_under_sandbox ())
    {
#ifdef HAVE_LIBPORTAL_GTK4
      peas_object_module_register_extension_type (module,
                                                  VALENT_TYPE_APPLICATION_PLUGIN,
                                                  VALENT_TYPE_XDP_BACKGROUND);
#endif /* HAVE_LIBPORTAL_GTK4 */

      peas_object_module_register_extension_type (module,
                                                  VALENT_TYPE_SESSION_ADAPTER,
                                                  VALENT_TYPE_XDP_SESSION);
    }
}

