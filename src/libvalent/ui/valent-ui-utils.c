// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-ui-utils"

#include "config.h"

#include <gdk-pixbuf/gdk-pixdata.h>

#include "valent-ui-utils.h"


/**
 * SECTION:valentuiutils
 * @short_description: Utilities for working with UI
 * @title: UI Utilities
 * @stability: Unstable
 * @include: libvalent-ui.h
 *
 * Helper functions and utilities for working with UI elements.
 */


/**
 * valent_ui_pixbuf_from_base64:
 * @base64: BASE64 encoded data
 * @error: (nullable): a #GError
 *
 * Create a new #GdkPixbuf for @base64.
 *
 * Returns: (transfer full) (nullable): a new #GdkPixbuf
 */
GdkPixbuf *
valent_ui_pixbuf_from_base64 (const char  *base64,
                              GError     **error)
{
  g_autoptr (GdkPixbufLoader) loader = NULL;
  g_autoptr (GdkPixbuf) pixbuf = NULL;
  g_autofree guchar *data = NULL;
  gsize dlen;

  data = g_base64_decode (base64, &dlen);

  loader = gdk_pixbuf_loader_new();

  if (gdk_pixbuf_loader_write (loader, data, dlen, error) &&
      gdk_pixbuf_loader_close (loader, error))
    pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);

  return g_steal_pointer (&pixbuf);
}

