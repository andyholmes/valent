// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>
// SPDX-FileContributor: 2018-2019 Christian Hergert <chergert@redhat.com>

#pragma once

#if !defined (VALENT_CORE_INSIDE) && !defined (VALENT_CORE_COMPILATION)
# error "Only <libvalent-core.h> can be included directly."
#endif

#ifndef __GI_SCANNER__

#include <glib.h>

#include "valent-global.h"

G_BEGIN_DECLS

/**
 * VALENT_IS_MAIN_THREAD: (skip)
 *
 * Get whether the current thread is the main thread.
 *
 * Returns: %TRUE if on the main thread, %FALSE on any other thread
 */
#define VALENT_IS_MAIN_THREAD() (g_thread_self() == valent_get_main_thread())

/**
 * valent_error_ignore: (skip)
 * @error: (nullable): a #GError
 *
 * Get if @error represents an ignorable error.
 *
 * This will return %TRUE if @error represents [type@Gio.IOErrorEnum.CANCELLED]
 * or [type@Gio.IOErrorEnum.NOT_SUPPORTED].
 *
 * Returns: %TRUE if non-fatal, %FALSE otherwise
 */
static inline gboolean
valent_error_ignore (const GError *error)
{
  return g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED) ||
         g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED);
}

/**
 * valent_set_string: (skip)
 * @ptr: a pointer to a string
 * @str: the string to set
 *
 * Set a string.
 *
 * Returns: %TRUE if changed, or %FALSE otherwise
 */
static inline gboolean
valent_set_string (char       **ptr,
                   const char  *str)
{
  char *copy;

  g_assert (ptr != NULL);

  if (*ptr == str || g_strcmp0 (*ptr, str) == 0)
    return FALSE;

  copy = g_strdup (str);
  g_free (*ptr);
  *ptr = copy;

  return TRUE;
}

G_END_DECLS

#endif /* __GI_SCANNER__ */

