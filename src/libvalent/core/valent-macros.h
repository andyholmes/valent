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
 * VALENT_NO_ASAN: (skip)
 *
 * A function attribute that disables AddressSanitizer.
 */
#define VALENT_NO_ASAN
#if defined(__has_attribute)
  #if __has_attribute(no_sanitize)
    #undef VALENT_NO_ASAN
    #define VALENT_NO_ASAN __attribute__((no_sanitize("address")))
  #endif
#endif

/**
 * VALENT_HAVE_ASAN: (skip)
 *
 * Defined to `1` if AddressSanitizer is enabled.
 */
#define VALENT_HAVE_ASAN 0
#if defined(__SANITIZE_ADDRESS__)
 #undef VALENT_HAVE_ASAN
 #define VALENT_HAVE_ASAN 1
#elif defined(__has_feature)
 #if __has_feature(address_sanitizer)
  #undef VALENT_HAVE_ASAN
  #define VALENT_HAVE_ASAN 1
 #endif
#endif

/**
 * VALENT_NO_TSAN: (skip)
 *
 * A function attribute that disables ThreadSanitizer.
 */
#define VALENT_NO_TSAN
#if defined(__has_attribute)
  #if __has_attribute(no_sanitize)
    #undef VALENT_NO_TSAN
    #define VALENT_NO_TSAN __attribute__((no_sanitize("thread")))
  #endif
#endif

/**
 * VALENT_HAVE_TSAN: (skip)
 *
 * Defined to `1` if ThreadSanitizer is enabled.
 */
#define VALENT_HAVE_TSAN 0
#if defined(__SANITIZE_THREAD__)
 #undef VALENT_HAVE_TSAN
 #define VALENT_HAVE_TSAN 1
#elif defined(__has_feature)
 #if __has_feature(thread_sanitizer)
  #undef VALENT_HAVE_TSAN
  #define VALENT_HAVE_TSAN 1
 #endif
#endif

/**
 * VALENT_NO_UBSAN: (skip)
 *
 * A function attribute that disables UndefinedBehaviourSanitizer.
 *
 * This macro only works on Clang.
 */
#define VALENT_NO_UBSAN
#if defined(__has_feature)
 #if __has_feature(undefined_sanitizer)
  #undef VALENT_NO_UBSAN
  #define VALENT_NO_UBSAN __attribute__((no_sanitize("undefined")))
 #endif
#endif

/**
 * VALENT_HAVE_UBSAN: (skip)
 *
 * Defined to `1` if UndefinedBehaviourSanitizer is enabled.
 *
 * This macro only works on Clang.
 */
#define VALENT_HAVE_UBSAN 0
#if defined(__has_feature)
 #if __has_feature(undefined_sanitizer)
  #undef VALENT_HAVE_UBSAN
  #define VALENT_HAVE_UBSAN 1
 #endif
#endif

G_END_DECLS

#endif /* __GI_SCANNER__ */

