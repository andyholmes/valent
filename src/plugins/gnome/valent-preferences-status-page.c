// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-device-preferences-dialog"

#include "config.h"

#include <glib/gi18n-lib.h>
#include <adwaita.h>
#include <gtk/gtk.h>
#include <valent.h>

#include "valent-preferences-status-page.h"


struct _ValentPreferencesStatusPage
{
  ValentPreferencesPage  parent_instance;

  /* template */
  AdwExpanderRow        *full_notification;
  GtkAdjustment         *full_notification_level;
  AdwExpanderRow        *low_notification;
  GtkAdjustment         *low_notification_level;

  AdwComboRow           *ringing_volume;
  GtkSwitch             *ringing_pause;
  AdwComboRow           *talking_volume;
  GtkSwitch             *talking_pause;
  GtkSwitch             *talking_microphone;
  GtkSwitch             *offline_notification;
};

G_DEFINE_FINAL_TYPE (ValentPreferencesStatusPage, valent_preferences_status_page, VALENT_TYPE_PREFERENCES_PAGE)

/* Mapping functions for converting our [-1...100] volume range to a boolean,
 * where -1 generally means "don't change", 0 means mute and any other value is
 * a volume percentage.
 * */
static gboolean
get_mute_volume_boolean (GValue   *value,
                         GVariant *variant,
                         gpointer  user_data)
{
  int volume;

  volume = g_variant_get_int32 (variant);

  g_value_set_boolean (value, (volume == 0));

  return TRUE;
}

static GVariant *
set_mute_volume_boolean (const GValue       *value,
                         const GVariantType *expected_type,
                         gpointer            user_data)
{
  gboolean mute_volume;

  mute_volume = g_value_get_boolean (value);

  if (mute_volume)
    return g_variant_new_int32 (0);
  else
    return g_variant_new_int32 (-1);
}

#define VOLUME_NOTHING -1
#define VOLUME_LOWER   15
#define VOLUME_MUTE     0

static gboolean
get_volume (GValue   *value,
            GVariant *variant,
            gpointer  user_data)
{
  switch (g_variant_get_int32 (variant))
    {
    case VOLUME_NOTHING:
      g_value_set_uint (value, 0);
      return TRUE;

    case VOLUME_LOWER:
      g_value_set_uint (value, 1);
      return TRUE;

    case VOLUME_MUTE:
      g_value_set_uint (value, 2);
      return TRUE;

    default:
      return FALSE;
    }
}

static GVariant *
set_volume (const GValue       *value,
            const GVariantType *expected_type,
            gpointer            user_data)
{
  switch (g_value_get_uint (value))
    {
    case 0:
      return g_variant_new_int32 (VOLUME_NOTHING);

    case 1:
      return g_variant_new_int32 (VOLUME_LOWER);

    case 2:
      return g_variant_new_int32 (VOLUME_MUTE);

    default:
      return g_variant_new_int32 (VOLUME_NOTHING);
    }
}

static inline void
valent_preferences_status_page_bind_context (ValentPreferencesStatusPage *self)
{
  ValentPreferencesPage *page = VALENT_PREFERENCES_PAGE (self);
  GSettings *settings = NULL;

  /* Battery
   */
  settings = valent_preferences_page_get_settings (page, "battery");
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

  /* Telephony
   */
  settings = valent_preferences_page_get_settings (page, "telephony");
  g_settings_bind (settings,            "ringing-pause",
                   self->ringing_pause, "active",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind_with_mapping (settings,             "ringing-volume",
                                self->ringing_volume, "selected",
                                G_SETTINGS_BIND_DEFAULT,
                                get_volume,
                                set_volume,
                                NULL, NULL);
  g_settings_bind_with_mapping (settings,                 "talking-microphone",
                                self->talking_microphone, "active",
                                G_SETTINGS_BIND_DEFAULT,
                                get_mute_volume_boolean,
                                set_mute_volume_boolean,
                                NULL, NULL);
  g_settings_bind_with_mapping (settings,             "talking-volume",
                                self->talking_volume, "selected",
                                G_SETTINGS_BIND_DEFAULT,
                                get_volume,
                                set_volume,
                                NULL, NULL);
  g_settings_bind (settings,            "talking-pause",
                   self->talking_pause, "active",
                   G_SETTINGS_BIND_DEFAULT);

  /* Connectivity
   */
  settings = valent_preferences_page_get_settings (page, "connectivity_report");
  g_settings_bind (settings,                   "offline-notification",
                   self->offline_notification, "active",
                   G_SETTINGS_BIND_DEFAULT);
}

/*
 * GObject
 */
static void
valent_preferences_status_page_dispose (GObject *object)
{
  GtkWidget *widget = GTK_WIDGET (object);

  gtk_widget_dispose_template (widget, VALENT_TYPE_PREFERENCES_STATUS_PAGE);

  G_OBJECT_CLASS (valent_preferences_status_page_parent_class)->dispose (object);
}

static void
valent_preferences_status_page_notify (GObject    *object,
                                     GParamSpec *pspec)
{
  ValentPreferencesPage *page = VALENT_PREFERENCES_PAGE (object);
  ValentPreferencesStatusPage *self = VALENT_PREFERENCES_STATUS_PAGE (object);

  if (g_strcmp0 (pspec->name, "context") == 0)
    {
      if (valent_preferences_page_get_context (page) != NULL)
        valent_preferences_status_page_bind_context (self);
    }

  if (G_OBJECT_CLASS (valent_preferences_status_page_parent_class)->notify)
    G_OBJECT_CLASS (valent_preferences_status_page_parent_class)->notify (object,
                                                                          pspec);
}

static void
valent_preferences_status_page_class_init (ValentPreferencesStatusPageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = valent_preferences_status_page_dispose;
  object_class->notify = valent_preferences_status_page_notify;

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/gnome/valent-preferences-status-page.ui");
  gtk_widget_class_bind_template_child (widget_class, ValentPreferencesStatusPage, full_notification);
  gtk_widget_class_bind_template_child (widget_class, ValentPreferencesStatusPage, full_notification_level);
  gtk_widget_class_bind_template_child (widget_class, ValentPreferencesStatusPage, low_notification);
  gtk_widget_class_bind_template_child (widget_class, ValentPreferencesStatusPage, low_notification_level);

  gtk_widget_class_bind_template_child (widget_class, ValentPreferencesStatusPage, ringing_volume);
  gtk_widget_class_bind_template_child (widget_class, ValentPreferencesStatusPage, ringing_pause);
  gtk_widget_class_bind_template_child (widget_class, ValentPreferencesStatusPage, talking_volume);
  gtk_widget_class_bind_template_child (widget_class, ValentPreferencesStatusPage, talking_pause);
  gtk_widget_class_bind_template_child (widget_class, ValentPreferencesStatusPage, talking_microphone);
  gtk_widget_class_bind_template_child (widget_class, ValentPreferencesStatusPage, offline_notification);
}

static void
valent_preferences_status_page_init (ValentPreferencesStatusPage *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

