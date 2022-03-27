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
  AdwPreferencesPage  parent_instance;

  char               *device_id;
  PeasPluginInfo     *plugin_info;
  GSettings          *settings;

  /* Template widgets */
  GtkSwitch          *auto_mount;
  AdwExpanderRow     *local_allow;
  GtkAdjustment      *local_port;
};

/* Interfaces */
static void valent_device_preferences_page_iface_init (ValentDevicePreferencesPageInterface *iface);

G_DEFINE_TYPE_WITH_CODE (ValentSftpPreferences, valent_sftp_preferences, ADW_TYPE_PREFERENCES_PAGE,
                         G_IMPLEMENT_INTERFACE (VALENT_TYPE_DEVICE_PREFERENCES_PAGE, valent_device_preferences_page_iface_init))

enum {
  PROP_0,
  PROP_DEVICE_ID,
  PROP_PLUGIN_INFO,
  N_PROPERTIES
};


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
 * ValentDevicePreferencesPage
 */
static void
valent_device_preferences_page_iface_init (ValentDevicePreferencesPageInterface *iface)
{
}


/*
 * GObject
 */
static void
valent_sftp_preferences_constructed (GObject *object)
{
  ValentSftpPreferences *self = VALENT_SFTP_PREFERENCES (object);

  /* Setup GSettings */
  self->settings = valent_device_plugin_new_settings (self->device_id,
                                                      "sftp");

  g_settings_bind (self->settings,   "auto-mount",
                   self->auto_mount, "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (self->settings,    "local-allow",
                   self->local_allow, "enable-expansion",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (self->settings,   "local-port",
                   self->local_port, "value",
                   G_SETTINGS_BIND_DEFAULT);

  G_OBJECT_CLASS (valent_sftp_preferences_parent_class)->constructed (object);
}

static void
valent_sftp_preferences_finalize (GObject *object)
{
  ValentSftpPreferences *self = VALENT_SFTP_PREFERENCES (object);

  g_clear_pointer (&self->device_id, g_free);
  g_clear_object (&self->settings);

  G_OBJECT_CLASS (valent_sftp_preferences_parent_class)->finalize (object);
}

static void
valent_sftp_preferences_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  ValentSftpPreferences *self = VALENT_SFTP_PREFERENCES (object);

  switch (prop_id)
    {
    case PROP_DEVICE_ID:
      g_value_set_string (value, self->device_id);
      break;

    case PROP_PLUGIN_INFO:
      g_value_set_boxed (value, self->plugin_info);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_sftp_preferences_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  ValentSftpPreferences *self = VALENT_SFTP_PREFERENCES (object);

  switch (prop_id)
    {
    case PROP_DEVICE_ID:
      self->device_id = g_value_dup_string (value);
      break;

    case PROP_PLUGIN_INFO:
      self->plugin_info = g_value_get_boxed (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_sftp_preferences_class_init (ValentSftpPreferencesClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = valent_sftp_preferences_constructed;
  object_class->finalize = valent_sftp_preferences_finalize;
  object_class->get_property = valent_sftp_preferences_get_property;
  object_class->set_property = valent_sftp_preferences_set_property;

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/sftp/valent-sftp-preferences.ui");
  gtk_widget_class_bind_template_child (widget_class, ValentSftpPreferences, auto_mount);
  gtk_widget_class_bind_template_child (widget_class, ValentSftpPreferences, local_allow);
  gtk_widget_class_bind_template_child (widget_class, ValentSftpPreferences, local_port);

  gtk_widget_class_bind_template_callback (widget_class, on_toggle_row);

  g_object_class_override_property (object_class, PROP_DEVICE_ID, "device-id");
  g_object_class_override_property (object_class, PROP_PLUGIN_INFO, "plugin-info");
}

static void
valent_sftp_preferences_init (ValentSftpPreferences *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

