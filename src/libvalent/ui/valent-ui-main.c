// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-ui-main"

#include "config.h"

#include <adwaita.h>
#include <gtk/gtk.h>
#include <libvalent-core.h>
#include <libvalent-media.h>

#include "valent-application.h"
#include "valent-application-plugin.h"

#include "valent-device-gadget.h"
#include "valent-device-page.h"
#include "valent-device-preferences-group.h"
#include "valent-device-preferences-window.h"
#include "valent-media-remote.h"
#include "valent-menu-list.h"
#include "valent-menu-stack.h"
#include "valent-preferences-page.h"
#include "valent-preferences-window.h"
#include "valent-window.h"
#include "valent-ui-utils.h"


static GtkWindow *media_remote = NULL;


/*
 * GActions
 */
static void
media_remote_action (GSimpleAction *action,
                     GVariant      *parameter,
                     gpointer       user_data)
{
  if (media_remote == NULL)
    {
      media_remote = g_object_new (VALENT_TYPE_MEDIA_REMOTE,
                                   "players", valent_media_get_default (),
                                   NULL);
      g_object_add_weak_pointer (G_OBJECT (media_remote),
                                 (gpointer) &media_remote);
    }

  gtk_window_present_with_time (media_remote, GDK_CURRENT_TIME);
}

static void
valent_ui_init_actions (void)
{
  GApplication *application = g_application_get_default ();
  static const GActionEntry actions[] = {
    { "media-remote", media_remote_action, NULL, NULL, NULL },
  };

  if (application != NULL)
    {
      g_action_map_add_action_entries (G_ACTION_MAP (application),
                                       actions,
                                       G_N_ELEMENTS (actions),
                                       application);
    }
}

static void
valent_ui_init_resources (void)
{
  g_autoptr (GtkCssProvider) theme = NULL;

  /* Custom CSS */
  theme = gtk_css_provider_new ();
  gtk_css_provider_load_from_resource (theme, "/ca/andyholmes/Valent/ui/style.css");
  gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                              GTK_STYLE_PROVIDER (theme),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

static void
valent_ui_init_types (void)
{
  g_type_ensure (VALENT_TYPE_APPLICATION);
  g_type_ensure (VALENT_TYPE_APPLICATION_PLUGIN);

  g_type_ensure (VALENT_TYPE_DEVICE_GADGET);
  g_type_ensure (VALENT_TYPE_DEVICE_PAGE);
  g_type_ensure (VALENT_TYPE_DEVICE_PREFERENCES_GROUP);
  g_type_ensure (VALENT_TYPE_DEVICE_PREFERENCES_WINDOW);
  g_type_ensure (VALENT_TYPE_MEDIA_REMOTE);
  g_type_ensure (VALENT_TYPE_MENU_LIST);
  g_type_ensure (VALENT_TYPE_MENU_STACK);
  g_type_ensure (VALENT_TYPE_PREFERENCES_PAGE);
  g_type_ensure (VALENT_TYPE_PREFERENCES_WINDOW);
  g_type_ensure (VALENT_TYPE_WINDOW);
}

/**
 * valent_ui_init:
 *
 * Initialize Valent's default user interface.
 *
 * Returns: %TRUE if successful, or %FALSE on failure
 *
 * Since: 1.0
 */
gboolean
valent_ui_init (void)
{
  static gboolean initialized = -1;

  if G_LIKELY (initialized != -1)
    return initialized;

  if ((initialized = gtk_init_check ()))
    {
      adw_init ();

      valent_ui_init_types ();
      valent_ui_init_resources ();
      valent_ui_init_actions ();
    }

  return initialized;
}
