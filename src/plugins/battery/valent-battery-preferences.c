// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-battery-preferences"

#include "config.h"

#include <glib/gi18n.h>
#include <libvalent-core.h>
#include <libvalent-device.h>
#include <libvalent-ui.h>

#include "valent-battery-preferences.h"


struct _ValentBatteryPreferences
{
  ValentDevicePreferencesPage  parent_instance;

  /* template */
  GtkSwitch                   *share_state;

  AdwExpanderRow              *full_notification;
  GtkAdjustment               *full_notification_level;
  AdwExpanderRow              *low_notification;
  GtkAdjustment               *low_notification_level;
};

G_DEFINE_FINAL_TYPE (ValentBatteryPreferences, valent_battery_preferences, VALENT_TYPE_DEVICE_PREFERENCES_PAGE)


/*
 * GObject
 */
static void
valent_battery_preferences_constructed (GObject *object)
{
  ValentBatteryPreferences *self = VALENT_BATTERY_PREFERENCES (object);
  ValentDevicePreferencesPage *page = VALENT_DEVICE_PREFERENCES_PAGE (self);
  GSettings *settings;

  /* Setup GSettings */
  settings = valent_device_preferences_page_get_settings (page);

  g_settings_bind (settings,          "share-state",
                   self->share_state, "active",
                   G_SETTINGS_BIND_DEFAULT);

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
valent_battery_preferences_class_init (ValentBatteryPreferencesClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = valent_battery_preferences_constructed;

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/battery/valent-battery-preferences.ui");
  gtk_widget_class_bind_template_child (widget_class, ValentBatteryPreferences, share_state);
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

