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
  static int host = -1;

  if (host != -1)
    return host;

  host = TRUE;

  if (valent_in_flatpak ())
    {
      int status = 0;

      g_spawn_command_line_sync ("flatpak-spawn --host echo",
                                 NULL, NULL, &status, NULL);
#if !GLIB_CHECK_VERSION(2, 69, 0)
      host = g_spawn_check_exit_status (status, NULL);
#else
      host = g_spawn_check_wait_status (status, NULL);
#endif
    }

  return host;
}

/**
 * valent_runcommand_can_spawn_sandbox:
 *
 * Check if `flatpak-spawn` can be found in `PATH`.
 *
 * Returns: %TRUE if available, %FALSE otherwise.
 */
gboolean
valent_runcommand_can_spawn_sandbox (void)
{
  static int sandbox = -1;
  g_autofree char *path = NULL;

  if (sandbox != -1)
    return sandbox;

  if ((path = g_find_program_in_path ("flatpak-spawn")) == NULL)
    sandbox = FALSE;
  else
    sandbox = TRUE;

  return sandbox;
}

