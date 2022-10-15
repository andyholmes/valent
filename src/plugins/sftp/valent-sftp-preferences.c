// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-sftp-preferences"

#include "config.h"

#include <glib/gi18n.h>
#include <libvalent-core.h>
#include <libvalent-ui.h>

#include "valent-sftp-preferences.h"


struct _ValentSftpPreferences
{
  ValentDevicePreferencesPage  parent_instance;

  /* template */
  GtkSwitch                   *auto_mount;
  AdwExpanderRow              *local_allow;
  GtkAdjustment               *local_port;
};

G_DEFINE_TYPE (ValentSftpPreferences, valent_sftp_preferences, VALENT_TYPE_DEVICE_PREFERENCES_PAGE)


static void
on_toggle_row (GtkListBox    *box,
               GtkListBoxRow *row,
               gpointer       user_data)
{
  gboolean active;
  GtkWidget *grid;
  GtkWidget *toggle;

  g_assert (GTK_IS_LIST_BOX (box));
  g_assert (GTK_IS_LIST_BOX_ROW (row));

  grid = gtk_list_box_row_get_child (row);
  toggle = gtk_grid_get_child_at (GTK_GRID (grid), 1, 0);

  g_object_get (toggle, "active", &active, NULL);
  g_object_set (toggle, "active", !active, NULL);
}


/*
 * GObject
 */
static void
valent_sftp_preferences_constructed (GObject *object)
{
  ValentSftpPreferences *self = VALENT_SFTP_PREFERENCES (object);
  ValentDevicePreferencesPage *page = VALENT_DEVICE_PREFERENCES_PAGE (self);
  GSettings *settings;

  /* Setup GSettings */
  settings = valent_device_preferences_page_get_settings (page);

  g_settings_bind (settings,         "auto-mount",
                   self->auto_mount, "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (settings,          "local-allow",
                   self->local_allow, "enable-expansion",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (settings,         "local-port",
                   self->local_port, "value",
                   G_SETTINGS_BIND_DEFAULT);

  G_OBJECT_CLASS (valent_sftp_preferences_parent_class)->constructed (object);
}

static void
valent_sftp_preferences_class_init (ValentSftpPreferencesClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = valent_sftp_preferences_constructed;

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/sftp/valent-sftp-preferences.ui");
  gtk_widget_class_bind_template_child (widget_class, ValentSftpPreferences, auto_mount);
  gtk_widget_class_bind_template_child (widget_class, ValentSftpPreferences, local_allow);
  gtk_widget_class_bind_template_child (widget_class, ValentSftpPreferences, local_port);

  gtk_widget_class_bind_template_callback (widget_class, on_toggle_row);
}

static void
valent_sftp_preferences_init (ValentSftpPreferences *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

