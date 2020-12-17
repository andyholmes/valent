// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-xdp-utils"

#include "config.h"

#include <libportal/portal.h>

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

