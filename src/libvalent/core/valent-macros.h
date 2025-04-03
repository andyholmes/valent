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

/**
 * VALENT_SANITIZE_ADDRESS: (skip)
 *
 * This macro is defined with value `1` when instrumented with
 * AddressSanitizer, otherwise `0`.
 */
#define VALENT_SANITIZE_ADDRESS 0
#if defined(__SANITIZE_ADDRESS__)
 #undef VALENT_SANITIZE_ADDRESS
 #define VALENT_SANITIZE_ADDRESS 1
#elif defined(__has_feature)
 #if __has_feature(address_sanitizer)
  #undef VALENT_SANITIZE_ADDRESS
  #define VALENT_SANITIZE_ADDRESS 1
 #endif
#endif

/**
 * VALENT_SANITIZE_THREAD: (skip)
 *
 * This macro is defined with value `1` when instrumented with
 * ThreadSanitizer, otherwise `0`.
 */
#define VALENT_SANITIZE_THREAD 0
#if defined(__SANITIZE_THREAD__)
 #undef VALENT_SANITIZE_THREAD
 #define VALENT_SANITIZE_THREAD 1
#elif defined(__has_feature)
 #if __has_feature(thread_sanitizer)
  #undef VALENT_SANITIZE_THREAD
  #define VALENT_SANITIZE_THREAD 1
 #endif
#endif

G_END_DECLS

#endif /* __GI_SCANNER__ */

