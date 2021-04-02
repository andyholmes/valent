// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include "config.h"

#include <glib/gi18n.h>
#include <libvalent-core.h>
#include <libvalent-ui.h>


int
main (int   argc,
      char *argv[])
{
  int ret;
  g_autoptr (ValentApplication) service = NULL;
  g_autofree char *application_name = NULL;

  /* Set up gettext translations */
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  application_name = g_strconcat ("Valent", PROFILE_SUFFIX, NULL);
  g_set_application_name (application_name);

  /* Start the service */
  valent_debug_init ();

  service = _valent_application_new ();
  ret = g_application_run (G_APPLICATION (service), argc, argv);

  valent_debug_clear ();

  return ret;
}
