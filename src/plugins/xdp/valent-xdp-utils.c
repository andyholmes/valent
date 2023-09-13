// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-xdp-utils"

#include "config.h"

#include <libportal/portal.h>
#ifdef HAVE_LIBPORTAL_GTK4
# include <gtk/gtk.h>
# include <libportal-gtk4/portal-gtk4.h>
#endif /* HAVE_LIBPORTAL_GTK4 */

#include "valent-xdp-utils.h"


static XdpPortal *default_portal = NULL;


#ifdef HAVE_LIBPORTAL_GTK4
static GtkWindow *
valent_xdp_get_active_window (void)
{
  GListModel *windows = NULL;
  unsigned int n_windows = 0;

  windows = gtk_window_get_toplevels ();
  n_windows = g_list_model_get_n_items (windows);

  for (unsigned int i = 0; i < n_windows; i++)
    {
      g_autoptr (GtkWindow) window = g_list_model_get_item (windows, i);

      if (gtk_window_is_active (window))
        return g_steal_pointer (&window);
    }

  return NULL;
}
#endif /* HAVE_LIBPORTAL_GTK4 */

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
 *
 * Get an [class@Xdp.Parent], if available.
 *
 * If Valent was compiled without support for libportal-gtk4 or there is no
 * active window, this function will return %NULL.
 *
 * Returns: (nullable) (transfer full): a `XdpParent`
 */
XdpParent *
valent_xdp_get_parent (void)
{
#ifdef HAVE_LIBPORTAL_GTK4
  g_autoptr (GtkWindow) window = NULL;

  window = valent_xdp_get_active_window ();

  if (window != NULL)
    return xdp_parent_new_gtk (window);
#endif /* HAVE_LIBPORTAL_GTK4 */

  return NULL;
}

/**
 * valent_xdp_has_parent:
 *
 * Check if an active parent is available.
 *
 * If Valent was compiled without support for libportal-gtk4 or there is no
 * active window, this function will return %FALSE.
 *
 * Returns: %TRUE if there is an active window, or %FALSE if not
 */
gboolean
valent_xdp_has_parent (void)
{
#ifdef HAVE_LIBPORTAL_GTK4
  g_autoptr (GtkWindow) window = NULL;

  window = valent_xdp_get_active_window ();

  if (window != NULL)
    return TRUE;
#endif /* HAVE_LIBPORTAL_GTK4 */

  return FALSE;
}

