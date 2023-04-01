// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-connectivity_report-preferences"

#include "config.h"

#include <glib/gi18n.h>
#include <adwaita.h>
#include <gtk/gtk.h>
#include <valent.h>

#include "valent-connectivity_report-preferences.h"


struct _ValentConnectivityReportPreferences
{
  ValentDevicePreferencesGroup  parent_instance;

  /* template */
  AdwActionRow                *share_state_row;
  GtkSwitch                   *share_state;

  AdwActionRow                *offline_notification_row;
  GtkSwitch                   *offline_notification;
};

G_DEFINE_FINAL_TYPE (ValentConnectivityReportPreferences, valent_connectivity_report_preferences, VALENT_TYPE_DEVICE_PREFERENCES_GROUP)


/*
 * GObject
 */
static void
valent_connectivity_report_preferences_constructed (GObject *object)
{
  ValentConnectivityReportPreferences *self = VALENT_CONNECTIVITY_REPORT_PREFERENCES (object);
  ValentDevicePreferencesGroup *group = VALENT_DEVICE_PREFERENCES_GROUP (self);
  GSettings *settings;

  settings = valent_device_preferences_group_get_settings (group);
  g_settings_bind (settings,          "share-state",
                   self->share_state, "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (settings,                   "offline-notification",
                   self->offline_notification, "active",
                   G_SETTINGS_BIND_DEFAULT);

  G_OBJECT_CLASS (valent_connectivity_report_preferences_parent_class)->constructed (object);
}

static void
valent_connectivity_report_preferences_dispose (GObject *object)
{
  GtkWidget *widget = GTK_WIDGET (object);

  gtk_widget_dispose_template (widget, VALENT_TYPE_CONNECTIVITY_REPORT_PREFERENCES);

  G_OBJECT_CLASS (valent_connectivity_report_preferences_parent_class)->dispose (object);
}

static void
valent_connectivity_report_preferences_class_init (ValentConnectivityReportPreferencesClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = valent_connectivity_report_preferences_constructed;
  object_class->dispose = valent_connectivity_report_preferences_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/connectivity_report/valent-connectivity_report-preferences.ui");
  gtk_widget_class_bind_template_child (widget_class, ValentConnectivityReportPreferences, share_state_row);
  gtk_widget_class_bind_template_child (widget_class, ValentConnectivityReportPreferences, share_state);
  gtk_widget_class_bind_template_child (widget_class, ValentConnectivityReportPreferences, offline_notification_row);
  gtk_widget_class_bind_template_child (widget_class, ValentConnectivityReportPreferences, offline_notification);
}

static void
valent_connectivity_report_preferences_init (ValentConnectivityReportPreferences *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

