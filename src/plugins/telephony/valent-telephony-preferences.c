// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-telephony-preferences"

#include "config.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libvalent-core.h>
#include <libvalent-ui.h>

#include "valent-telephony-preferences.h"


struct _ValentTelephonyPreferences
{
  ValentDevicePreferencesPage  parent_instance;

  /* template */
  AdwPreferencesGroup         *ringing_group;
  AdwComboRow                 *ringing_volume;
  GtkSwitch                   *ringing_pause;

  AdwPreferencesGroup         *talking_group;
  AdwComboRow                 *talking_volume;
  GtkSwitch                   *talking_pause;
  GtkSwitch                   *talking_microphone;
};

G_DEFINE_TYPE (ValentTelephonyPreferences, valent_telephony_preferences, VALENT_TYPE_DEVICE_PREFERENCES_PAGE)


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
  int volume;
  guint selected;

  volume = g_variant_get_int32 (variant);

  switch (volume)
    {
    case VOLUME_NOTHING:
      selected = 0;
      break;

    case VOLUME_LOWER:
      selected = 1;
      break;

    case VOLUME_MUTE:
      selected = 2;
      break;

    default:
      selected = GTK_INVALID_LIST_POSITION;
    }

  g_value_set_uint (value, selected);

  return TRUE;
}

static GVariant *
set_volume (const GValue       *value,
            const GVariantType *expected_type,
            gpointer            user_data)
{
  unsigned int selected;

  selected = g_value_get_uint (value);

  switch (selected)
    {
    case 0:
      return g_variant_new_int32 (-1);

    case 1:
      return g_variant_new_int32 (15);

    case 2:
      return g_variant_new_int32 (0);

    default:
      return g_variant_new_int32 (-1);
    }
}


/*
 * GObject
 */
static void
valent_telephony_preferences_constructed (GObject *object)
{
  ValentTelephonyPreferences *self = VALENT_TELEPHONY_PREFERENCES (object);
  ValentDevicePreferencesPage *page = VALENT_DEVICE_PREFERENCES_PAGE (self);
  GSettings *settings;

  /* Setup GSettings */
  settings = valent_device_preferences_page_get_settings (page);

  /* Incoming Calls */
  g_settings_bind (settings,            "ringing-pause",
                   self->ringing_pause, "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind_with_mapping (settings,             "ringing-volume",
                                self->ringing_volume, "selected",
                                G_SETTINGS_BIND_DEFAULT,
                                get_volume,
                                set_volume,
                                NULL, NULL);

  /* Ongoing Calls */
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

  G_OBJECT_CLASS (valent_telephony_preferences_parent_class)->constructed (object);
}

static void
valent_telephony_preferences_class_init (ValentTelephonyPreferencesClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = valent_telephony_preferences_constructed;

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/telephony/valent-telephony-preferences.ui");
  gtk_widget_class_bind_template_child (widget_class, ValentTelephonyPreferences, ringing_group);
  gtk_widget_class_bind_template_child (widget_class, ValentTelephonyPreferences, ringing_volume);
  gtk_widget_class_bind_template_child (widget_class, ValentTelephonyPreferences, ringing_pause);
  gtk_widget_class_bind_template_child (widget_class, ValentTelephonyPreferences, talking_group);
  gtk_widget_class_bind_template_child (widget_class, ValentTelephonyPreferences, talking_volume);
  gtk_widget_class_bind_template_child (widget_class, ValentTelephonyPreferences, talking_pause);
  gtk_widget_class_bind_template_child (widget_class, ValentTelephonyPreferences, talking_microphone);
}

static void
valent_telephony_preferences_init (ValentTelephonyPreferences *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

