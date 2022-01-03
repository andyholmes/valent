// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-runcommand-utils"

#include "config.h"

#include <gio/gio.h>
#include <libvalent-core.h>

#include "valent-runcommand-utils.h"


/**
 * valent_runcommand_can_spawn_host:
 *
 * Check if subprocesses can be spawned on the host system.
 *
 * Returns: %TRUE if available, %FALSE otherwise.
 */
gboolean
valent_runcommand_can_spawn_host (void)
{
  static gboolean host = TRUE;
  static gsize guard = 0;

  if (g_once_init_enter (&guard))
    {
      if (valent_in_flatpak ())
        {
          int status = 0;

          g_spawn_command_line_sync ("flatpak-spawn --host true",
                                     NULL, NULL, &status, NULL);
#if GLIB_CHECK_VERSION(2, 70, 0)
          host = g_spawn_check_wait_status (status, NULL);
#else
          host = g_spawn_check_exit_status (status, NULL);
#endif
        }

      g_once_init_leave (&guard, 1);
    }

  return host;
}

