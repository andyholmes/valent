// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-xdp-utils"

#include "config.h"

#include <libportal/portal.h>
#ifdef HAVE_LIBPORTAL_GTK4
# include <libportal-gtk4/portal-gtk4.h>
#endif /* HAVE_LIBPORTAL_GTK4 */

#include "valent-xdp-utils.h"


static XdpPortal *default_portal = NULL;


/**
 * valent_xdp_get_default:
 *
 * Get the default #XdpPortal object for Valent.
 *
 * Returns: (transfer none): a #XdpPortal
 */
XdpPortal *
valent_xdp_get_default (void)
{
  if (default_portal == NULL)
    default_portal = xdp_portal_new ();

  return default_portal;
}

/**
 * valent_xdp_get_parent:
 * @application: (nullable): a #GApplication
 *
 * Get an #XdpParent, if available. If Valent was compiled without support for
 * libportal-gtk4 or there is no active window, this function will return %NULL.
 *
 * Returns: (nullable) (transfer full): a #XdpParent
 */
XdpParent *
valent_xdp_get_parent (GApplication *application)
{
  XdpParent *parent = NULL;

  g_return_val_if_fail (application == NULL || G_IS_APPLICATION (application), NULL);

#ifdef HAVE_LIBPORTAL_GTK4
  if (application == NULL)
    application = g_application_get_default ();

  if (GTK_IS_APPLICATION (application))
    {
      GtkWindow *window = NULL;

      window = gtk_application_get_active_window (GTK_APPLICATION (application));

      if (window != NULL)
        parent = xdp_parent_new_gtk (window);
    }
#endif /* HAVE_LIBPORTAL_GTK4 */

  return parent;
}

