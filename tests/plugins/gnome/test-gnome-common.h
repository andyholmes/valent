// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <locale.h>

#include <gtk/gtk.h>
#include <valent.h>
#include <libvalent-test.h>

#include "valent-ui-utils-private.h"

#pragma once

static inline void
valent_test_gnome_init (int    *argcp,
                        char ***argvp,
                        ...)
{
  valent_test_init (argcp, argvp, NULL);

  gtk_disable_setlocale ();
  setlocale (LC_ALL, "en_US.UTF-8");
  valent_ui_init ();

  /* NOTE: Set manually since GDK_DEBUG=default-settings doesn't work for us */
  g_object_set (gtk_settings_get_default (),
                "gtk-enable-animations", FALSE,
                NULL);
}

