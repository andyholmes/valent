// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include "config.h"

#include <glib.h>

#include "valent-version.h"


/**
 * valent_check_version:
 * @major: required major version
 * @minor: required minor version
 *
 * Run-time version check.
 *
 * Evaluates to %TRUE if the API version of libvalent is greater than or equal
 * to the required one.
 *
 * Returns: %TRUE if the requirement is met, or %FALSE if not
 *
 * Since: 1.0
 */
gboolean
valent_check_version (unsigned int major,
                      unsigned int minor)
{
  if (VALENT_MAJOR_VERSION > major)
    return TRUE;

  if (VALENT_MAJOR_VERSION == major && VALENT_MINOR_VERSION >= minor)
    return TRUE;

  return FALSE;
}

/**
 * valent_get_major_version:
 *
 * Get the major version component of the Valent library.
 *
 * For example, if the version `1.2` this is `1`.
 *
 * Returns: the major version component of libvalent
 *
 * Since: 1.0
 */
unsigned int
valent_get_major_version (void)
{
  return VALENT_MAJOR_VERSION;
}

/**
 * valent_get_minor_version:
 *
 * Get the minor version component of the Valent library.
 *
 * For example, if the version `1.2` this is `2`.
 *
 * Returns: the minor version component of libvalent
 *
 * Since: 1.0
 */
unsigned int
valent_get_minor_version (void)
{
  return VALENT_MINOR_VERSION;
}

/**
 * valent_get_micro_version:
 *
 * Get the micro version component of the Valent library.
 *
 * For example, if the version `1.2.3` this is `3`.
 *
 * Returns: the micro version component of libvalent
 *
 * Since: 1.0
 */
unsigned int
valent_get_micro_version (void)
{
  return VALENT_MICRO_VERSION;
}

