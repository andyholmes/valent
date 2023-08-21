// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include "config.h"

#include <glib/gi18n.h>
#include <libportal/portal.h>
#include <valent.h>


static void
valent_plugin_init (void)
{
  PeasEngine *engine = peas_engine_get_default ();
  g_autofree char *xdg_plugin_dir = NULL;

  /* The package plugin directory, typically `$LIBDIR/valent/plugins`. */
  peas_engine_add_search_path (engine, VALENT_PLUGINSDIR, NULL);

  /* The user plugin directory as reported by XDG directories. If in a Flatpak,
   * this will be `~/.var/app/APPLICATION_ID/data/PACKAGE_NAME/plugins`. */
  xdg_plugin_dir = g_build_filename (g_get_user_data_dir (),
                                     PACKAGE_NAME, "plugins", NULL);
  peas_engine_add_search_path (engine, xdg_plugin_dir, NULL);

  /* The real user plugin directory, regardless of XDG environment variables.
   * This will always be `~/.local/share/PACKAGE_NAME/plugins`. */
  if (xdp_portal_running_under_flatpak ())
    {
      g_autofree char *real_plugin_dir = NULL;

      real_plugin_dir = g_build_filename (g_get_home_dir (), ".local", "share",
                                          PACKAGE_NAME, "plugins", NULL);
      peas_engine_add_search_path (engine, real_plugin_dir, NULL);
    }
}

int
main (int   argc,
      char *argv[])
{
  int ret;
  g_autoptr (GApplication) service = NULL;

  /* Initialize Translations */
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  /* Initialize Valent */
  valent_debug_init ();
  valent_plugin_init ();

  if (g_getenv ("VALENT_HEADLESS") != NULL || !valent_ui_init ())
    g_debug ("Valent running in headless mode");

  /* Run and cleanup, before returning */
  g_set_application_name ("Valent");
  service = _valent_application_new ();
  ret = g_application_run (G_APPLICATION (service), argc, argv);

  valent_debug_clear ();

  return ret;
}
