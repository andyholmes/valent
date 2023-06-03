// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>
// SPDX-FileContributor: 2018-2019 Christian Hergert <chergert@redhat.com>

#pragma once

#if !defined (VALENT_INSIDE) && !defined (VALENT_COMPILATION)
# error "Only <valent.h> can be included directly."
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
 * VALENT_STRV_INIT: (skip)
 *
 * Initialize a static, %NULL-terminated character array.
 *
 * Returns: (transfer none): an array of strings
 */
#define VALENT_STRV_INIT(...) ((const char * const[]) { __VA_ARGS__, NULL})

G_END_DECLS

#endif /* __GI_SCANNER__ */

