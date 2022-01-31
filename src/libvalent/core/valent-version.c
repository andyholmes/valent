// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include "config.h"

#include <glib.h>

#include "valent-version.h"


/**
 * valent_check_version:
 * @major: required major version
 * @minor: required minor version
 *
 * Run-time version check. Evaluates to %TRUE if the running version of
 * Valent is greater than or equal to the required one.
 *
 * Returns: %TRUE if the requirement is met, or %FALSE if not
 */
gboolean
valent_check_version (unsigned int major,
                      unsigned int minor)
{
  if (major > VALENT_MAJOR_VERSION)
    return TRUE;

  if (major == VALENT_MAJOR_VERSION && minor >= VALENT_MINOR_VERSION)
    return TRUE;

  return FALSE;
}

/**
 * valent_get_major_version:
 *
 * Get the major version component of the Valent library. For example, if the
 * version `1.2` this is `1`.
 *
 * Returns: the major version component of libvalent
 */
unsigned int
valent_get_major_version (void)
{
  return VALENT_MAJOR_VERSION;
}

/**
 * valent_get_minor_version:
 *
 * Get the minor version component of the Valent library. For example, if the
 * version `1.2` this is `2`.
 *
 * Returns: the minor version component of libvalent
 */
unsigned int
valent_get_minor_version (void)
{
  return VALENT_MINOR_VERSION;
}

