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
  AdwPreferencesPage   parent_instance;

  GSettings           *settings;
  char                *plugin_context;

  /* Template widgets */
  AdwPreferencesGroup *ringing_group;
  AdwComboRow         *ringing_volume;
  GtkSwitch           *ringing_pause;

  AdwPreferencesGroup *talking_group;
  AdwComboRow         *talking_volume;
  GtkSwitch           *talking_pause;
  GtkSwitch           *talking_microphone;
};

/* Interfaces */
static void valent_plugin_preferences_iface_init (ValentPluginPreferencesInterface *iface);

G_DEFINE_TYPE_WITH_CODE (ValentTelephonyPreferences, valent_telephony_preferences, ADW_TYPE_PREFERENCES_PAGE,
                         G_IMPLEMENT_INTERFACE (VALENT_TYPE_PLUGIN_PREFERENCES, valent_plugin_preferences_iface_init))

enum {
  PROP_0,
  PROP_PLUGIN_CONTEXT,
  N_PROPERTIES
};


/* Mapping functions for converting our [-1...100] volume range to a boolean,
 * where -1 generally means "don't change", 0 means mute and any other value is
 * a volume percentage.
 * */
static gboolean
get_mute_volume_boolean (GValue   *value,
                         GVariant *variant,
                         gpointer  user_data)
{
  gint volume;

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
  gint volume;
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
 * ValentPluginPreferences
 */
static void
valent_plugin_preferences_iface_init (ValentPluginPreferencesInterface *iface)
{
}


/*
 * GObject
 */
static void
valent_telephony_preferences_constructed (GObject *object)
{
  ValentTelephonyPreferences *self = VALENT_TELEPHONY_PREFERENCES (object);

  /* Setup GSettings */
  self->settings = valent_device_plugin_new_settings (self->plugin_context,
                                                      "telephony");

  /* Incoming Calls */
  g_settings_bind (self->settings,      "ringing-pause",
                   self->ringing_pause, "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind_with_mapping (self->settings,       "ringing-volume",
                                self->ringing_volume, "selected",
                                G_SETTINGS_BIND_DEFAULT,
                                get_volume,
                                set_volume,
                                NULL, NULL);

  /* Ongoing Calls */
  g_settings_bind_with_mapping (self->settings,           "talking-microphone",
                                self->talking_microphone, "active",
                                G_SETTINGS_BIND_DEFAULT,
                                get_mute_volume_boolean,
                                set_mute_volume_boolean,
                                NULL, NULL);

  g_settings_bind_with_mapping (self->settings,       "talking-volume",
                                self->talking_volume, "selected",
                                G_SETTINGS_BIND_DEFAULT,
                                get_volume,
                                set_volume,
                                NULL, NULL);

  g_settings_bind (self->settings,      "talking-pause",
                   self->talking_pause, "active",
                   G_SETTINGS_BIND_DEFAULT);

  G_OBJECT_CLASS (valent_telephony_preferences_parent_class)->constructed (object);
}

static void
valent_telephony_preferences_finalize (GObject *object)
{
  ValentTelephonyPreferences *self = VALENT_TELEPHONY_PREFERENCES (object);

  g_clear_pointer (&self->plugin_context, g_free);
  g_clear_object (&self->settings);

  G_OBJECT_CLASS (valent_telephony_preferences_parent_class)->finalize (object);
}

static void
valent_telephony_preferences_get_property (GObject    *object,
                                           guint       prop_id,
                                           GValue     *value,
                                           GParamSpec *pspec)
{
  ValentTelephonyPreferences *self = VALENT_TELEPHONY_PREFERENCES (object);

  switch (prop_id)
    {
    case PROP_PLUGIN_CONTEXT:
      g_value_set_string (value, self->plugin_context);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_telephony_preferences_set_property (GObject      *object,
                                           guint         prop_id,
                                           const GValue *value,
                                           GParamSpec   *pspec)
{
  ValentTelephonyPreferences *self = VALENT_TELEPHONY_PREFERENCES (object);

  switch (prop_id)
    {
    case PROP_PLUGIN_CONTEXT:
      self->plugin_context = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_telephony_preferences_class_init (ValentTelephonyPreferencesClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = valent_telephony_preferences_get_property;
  object_class->set_property = valent_telephony_preferences_set_property;
  object_class->constructed = valent_telephony_preferences_constructed;
  object_class->finalize = valent_telephony_preferences_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/telephony/valent-telephony-preferences.ui");
  gtk_widget_class_bind_template_child (widget_class, ValentTelephonyPreferences, ringing_group);
  gtk_widget_class_bind_template_child (widget_class, ValentTelephonyPreferences, ringing_volume);
  gtk_widget_class_bind_template_child (widget_class, ValentTelephonyPreferences, ringing_pause);
  gtk_widget_class_bind_template_child (widget_class, ValentTelephonyPreferences, talking_group);
  gtk_widget_class_bind_template_child (widget_class, ValentTelephonyPreferences, talking_volume);
  gtk_widget_class_bind_template_child (widget_class, ValentTelephonyPreferences, talking_pause);
  gtk_widget_class_bind_template_child (widget_class, ValentTelephonyPreferences, talking_microphone);

  g_object_class_override_property (object_class,
                                    PROP_PLUGIN_CONTEXT,
                                    "plugin-context");
}

static void
valent_telephony_preferences_init (ValentTelephonyPreferences *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

