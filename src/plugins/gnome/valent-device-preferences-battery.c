// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-battery-preferences"

#include "config.h"

#include <glib/gi18n.h>
#include <adwaita.h>
#include <gtk/gtk.h>
#include <valent.h>

#include "valent-device-preferences-battery.h"


struct _ValentBatteryPreferences
{
  ValentDevicePreferencesGroup  parent_instance;

  /* template */
  AdwExpanderRow               *full_notification;
  GtkAdjustment                *full_notification_level;
  AdwExpanderRow               *low_notification;
  GtkAdjustment                *low_notification_level;
};

G_DEFINE_FINAL_TYPE (ValentBatteryPreferences, valent_battery_preferences, VALENT_TYPE_DEVICE_PREFERENCES_GROUP)


/*
 * GObject
 */
static void
valent_battery_preferences_constructed (GObject *object)
{
  ValentBatteryPreferences *self = VALENT_BATTERY_PREFERENCES (object);
  ValentDevicePreferencesGroup *group = VALENT_DEVICE_PREFERENCES_GROUP (self);
  GSettings *settings;

  settings = valent_device_preferences_group_get_settings (group);
  g_settings_bind (settings,                "full-notification",
                   self->full_notification, "enable-expansion",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (settings,                      "full-notification-level",
                   self->full_notification_level, "value",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (settings,               "low-notification",
                   self->low_notification, "enable-expansion",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (settings,                     "low-notification-level",
                   self->low_notification_level, "value",
                   G_SETTINGS_BIND_DEFAULT);

  G_OBJECT_CLASS (valent_battery_preferences_parent_class)->constructed (object);
}

static void
valent_battery_preferences_dispose (GObject *object)
{
  GtkWidget *widget = GTK_WIDGET (object);

  gtk_widget_dispose_template (widget, VALENT_TYPE_BATTERY_PREFERENCES);

  G_OBJECT_CLASS (valent_battery_preferences_parent_class)->dispose (object);
}

static void
valent_battery_preferences_class_init (ValentBatteryPreferencesClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = valent_battery_preferences_constructed;
  object_class->dispose = valent_battery_preferences_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/gnome/valent-device-preferences-battery.ui");
  gtk_widget_class_bind_template_child (widget_class, ValentBatteryPreferences, full_notification);
  gtk_widget_class_bind_template_child (widget_class, ValentBatteryPreferences, full_notification_level);
  gtk_widget_class_bind_template_child (widget_class, ValentBatteryPreferences, low_notification);
  gtk_widget_class_bind_template_child (widget_class, ValentBatteryPreferences, low_notification_level);
}

static void
valent_battery_preferences_init (ValentBatteryPreferences *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

