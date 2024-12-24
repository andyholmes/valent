// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-device-preferences"

#include "config.h"

#include <gio/gio.h>
#include <gtk/gtk.h>
#include <adwaita.h>
#include <valent.h>

#include "valent-device-preferences-group.h"

typedef struct
{
  ValentDataSource *data_source;
  PeasPluginInfo   *plugin_info;
  GSettings        *settings;
} ValentDevicePreferencesGroupPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (ValentDevicePreferencesGroup, valent_device_preferences_group, ADW_TYPE_PREFERENCES_GROUP)

typedef enum {
  PROP_DEVICE = 1,
  PROP_PLUGIN_INFO,
  PROP_SETTINGS,
} ValentDevicePreferencesGroupProperty;

static GParamSpec *properties[PROP_SETTINGS + 1] = { NULL, };


/*
 * GObject
 */
static void
valent_device_preferences_group_constructed (GObject *object)
{
  ValentDevicePreferencesGroup *self = VALENT_DEVICE_PREFERENCES_GROUP (object);
  ValentDevicePreferencesGroupPrivate *priv = valent_device_preferences_group_get_instance_private (self);

  G_OBJECT_CLASS (valent_device_preferences_group_parent_class)->constructed (object);

  if (priv->data_source == NULL)
    {
      ValentResource *resource = valent_data_source_get_local_default ();
      priv->data_source = VALENT_DATA_SOURCE (g_object_ref (resource));
    }

  if (priv->plugin_info != NULL)
    {
      priv->settings = valent_data_source_get_plugin_settings (priv->data_source,
                                                               priv->plugin_info,
                                                               "X-DevicePluginSettings",
                                                               "device");
    }
}

static void
valent_device_preferences_group_finalize (GObject *object)
{
  ValentDevicePreferencesGroup *self = VALENT_DEVICE_PREFERENCES_GROUP (object);
  ValentDevicePreferencesGroupPrivate *priv = valent_device_preferences_group_get_instance_private (self);

  g_clear_object (&priv->data_source);
  g_clear_object (&priv->plugin_info);
  g_clear_object (&priv->settings);

  G_OBJECT_CLASS (valent_device_preferences_group_parent_class)->finalize (object);
}

static void
valent_device_preferences_group_get_property (GObject    *object,
                                              guint       prop_id,
                                              GValue     *value,
                                              GParamSpec *pspec)
{
  ValentDevicePreferencesGroup *self = VALENT_DEVICE_PREFERENCES_GROUP (object);
  ValentDevicePreferencesGroupPrivate *priv = valent_device_preferences_group_get_instance_private (self);

  switch ((ValentDevicePreferencesGroupProperty)prop_id)
    {
    case PROP_DEVICE:
      g_value_set_object (value, priv->data_source);
      break;

    case PROP_PLUGIN_INFO:
      g_value_set_object (value, priv->plugin_info);
      break;

    case PROP_SETTINGS:
      g_value_set_object (value, priv->settings);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_device_preferences_group_set_property (GObject      *object,
                                              guint         prop_id,
                                              const GValue *value,
                                              GParamSpec   *pspec)
{
  ValentDevicePreferencesGroup *self = VALENT_DEVICE_PREFERENCES_GROUP (object);
  ValentDevicePreferencesGroupPrivate *priv = valent_device_preferences_group_get_instance_private (self);

  switch ((ValentDevicePreferencesGroupProperty)prop_id)
    {
    case PROP_DEVICE:
      priv->data_source = g_value_dup_object (value);
      break;

    case PROP_PLUGIN_INFO:
      priv->plugin_info = g_value_dup_object (value);
      break;

    case PROP_SETTINGS:
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_device_preferences_group_class_init (ValentDevicePreferencesGroupClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = valent_device_preferences_group_constructed;
  object_class->finalize = valent_device_preferences_group_finalize;
  object_class->get_property = valent_device_preferences_group_get_property;
  object_class->set_property = valent_device_preferences_group_set_property;

  /**
   * ValentDevicePreferencesGroup:device:
   *
   * The [class@Valent.DataSource] for the [class@Valent.DevicePlugin].
   */
  properties [PROP_DEVICE] =
    g_param_spec_object ("device", NULL, NULL,
                         VALENT_TYPE_DEVICE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentDevicePreferencesGroup:plugin-info:
   *
   * The [class@Peas.PluginInfo] describing this plugin.
   */
  properties [PROP_PLUGIN_INFO] =
    g_param_spec_object ("plugin-info", NULL, NULL,
                         PEAS_TYPE_PLUGIN_INFO,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentDevicePreferencesGroup:settings: (getter get_settings)
   *
   * The [class@Gio.Settings] for the [class@Valent.DevicePlugin].
   */
  properties [PROP_SETTINGS] =
    g_param_spec_object ("settings", NULL, NULL,
                         G_TYPE_SETTINGS,
                         (G_PARAM_READABLE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, G_N_ELEMENTS (properties), properties);
}

static void
valent_device_preferences_group_init (ValentDevicePreferencesGroup *self)
{
}

/**
 * valent_device_preferences_group_get_settings:
 * @group: a `ValentDevicePreferencesGroup`
 *
 * Get the [class@Gio.Settings] for the [class@Valent.DevicePlugin].
 *
 * Returns: (transfer none) (nullable): a `GSettings`
 */
GSettings *
valent_device_preferences_group_get_settings (ValentDevicePreferencesGroup *group)
{
  ValentDevicePreferencesGroupPrivate *priv = valent_device_preferences_group_get_instance_private (group);

  g_return_val_if_fail (VALENT_IS_DEVICE_PREFERENCES_GROUP (group), NULL);

  return priv->settings;
}

