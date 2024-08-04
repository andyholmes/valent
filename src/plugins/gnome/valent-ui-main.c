// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-ui-main"

#include "config.h"

#include <adwaita.h>
#include <gtk/gtk.h>

#include "valent-device-page.h"
#include "valent-device-preferences-battery.h"
#include "valent-device-preferences-clipboard.h"
#include "valent-device-preferences-commands.h"
#include "valent-device-preferences-connectivity.h"
#include "valent-device-preferences-contacts.h"
#include "valent-device-preferences-notification.h"
#include "valent-device-preferences-sftp.h"
#include "valent-device-preferences-share.h"
#include "valent-device-preferences-telephony.h"
#include "valent-device-preferences-group.h"
#include "valent-device-preferences-dialog.h"
#include "valent-input-remote.h"
#include "valent-media-remote.h"
#include "valent-menu-list.h"
#include "valent-menu-stack.h"
#include "valent-preferences-dialog.h"
#include "valent-ui-manager.h"
#include "valent-ui-utils-private.h"
#include "valent-window.h"


static void
valent_ui_init_resources (void)
{
  g_autoptr (GtkCssProvider) css_theme = NULL;
  g_autoptr (GtkIconTheme) icon_theme = NULL;

  css_theme = gtk_css_provider_new ();
  gtk_css_provider_load_from_resource (css_theme, "/plugins/gnome/style.css");
  gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                              GTK_STYLE_PROVIDER (css_theme),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  icon_theme = gtk_icon_theme_get_for_display (gdk_display_get_default ());
  gtk_icon_theme_add_resource_path (icon_theme, "/ca/andyholmes/Valent/icons");
}

static void
valent_ui_init_types (void)
{
  g_type_ensure (VALENT_TYPE_DEVICE_PAGE);
  g_type_ensure (VALENT_TYPE_DEVICE_PREFERENCES_GROUP);
  g_type_ensure (VALENT_TYPE_DEVICE_PREFERENCES_DIALOG);
  g_type_ensure (VALENT_TYPE_BATTERY_PREFERENCES);
  g_type_ensure (VALENT_TYPE_CLIPBOARD_PREFERENCES);
  g_type_ensure (VALENT_TYPE_RUNCOMMAND_PREFERENCES);
  g_type_ensure (VALENT_TYPE_CONNECTIVITY_REPORT_PREFERENCES);
  g_type_ensure (VALENT_TYPE_CONTACTS_PREFERENCES);
  g_type_ensure (VALENT_TYPE_NOTIFICATION_PREFERENCES);
  g_type_ensure (VALENT_TYPE_SFTP_PREFERENCES);
  g_type_ensure (VALENT_TYPE_SHARE_PREFERENCES);
  g_type_ensure (VALENT_TYPE_TELEPHONY_PREFERENCES);
  g_type_ensure (VALENT_TYPE_INPUT_REMOTE);
  g_type_ensure (VALENT_TYPE_MEDIA_REMOTE);
  g_type_ensure (VALENT_TYPE_MENU_LIST);
  g_type_ensure (VALENT_TYPE_MENU_STACK);
  g_type_ensure (VALENT_TYPE_PREFERENCES_DIALOG);
  g_type_ensure (VALENT_TYPE_UI_MANAGER);
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
    }

  return initialized;
}
