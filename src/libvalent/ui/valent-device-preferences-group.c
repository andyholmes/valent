// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-device-preferences"

#include "config.h"

#include <gio/gio.h>
#include <gtk/gtk.h>
#include <adwaita.h>
#include <libvalent-core.h>
#include <libvalent-device.h>

#include "valent-device-preferences-group.h"

typedef struct
{
  ValentContext  *context;
  PeasPluginInfo *plugin_info;
  GSettings      *settings;
} ValentDevicePreferencesGroupPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (ValentDevicePreferencesGroup, valent_device_preferences_group, ADW_TYPE_PREFERENCES_GROUP)

enum {
  PROP_0,
  PROP_CONTEXT,
  PROP_PLUGIN_INFO,
  PROP_SETTINGS,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };


/*
 * GObject
 */
static void
valent_device_preferences_group_finalize (GObject *object)
{
  ValentDevicePreferencesGroup *self = VALENT_DEVICE_PREFERENCES_GROUP (object);
  ValentDevicePreferencesGroupPrivate *priv = valent_device_preferences_group_get_instance_private (self);

  g_clear_object (&priv->context);
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

  switch (prop_id)
    {
    case PROP_CONTEXT:
      g_value_set_object (value, valent_device_preferences_group_get_context (self));
      break;

    case PROP_PLUGIN_INFO:
      g_value_set_object (value, priv->plugin_info);
      break;

    case PROP_SETTINGS:
      g_value_set_object (value, valent_device_preferences_group_get_settings (self));
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

  switch (prop_id)
    {
    case PROP_CONTEXT:
      priv->context = g_value_dup_object (value);
      break;

    case PROP_PLUGIN_INFO:
      priv->plugin_info = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_device_preferences_group_class_init (ValentDevicePreferencesGroupClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = valent_device_preferences_group_finalize;
  object_class->get_property = valent_device_preferences_group_get_property;
  object_class->set_property = valent_device_preferences_group_set_property;

  /**
   * ValentDevicePreferencesGroup:context: (getter get_context)
   *
   * The [class@Valent.Context] for the [class@Valent.DevicePlugin].
   */
  properties [PROP_CONTEXT] =
    g_param_spec_object ("context", NULL, NULL,
                         VALENT_TYPE_CONTEXT,
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

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
valent_device_preferences_group_init (ValentDevicePreferencesGroup *self)
{
}

/**
 * valent_device_preferences_group_get_context:
 * @group: a `ValentDevicePreferencesGroup`
 *
 * Get the [class@Valent.Context] for the [class@Valent.DevicePlugin].
 *
 * Returns: (transfer none) (nullable): a `ValentContext`
 */
ValentContext *
valent_device_preferences_group_get_context (ValentDevicePreferencesGroup *group)
{
  ValentDevicePreferencesGroupPrivate *priv = valent_device_preferences_group_get_instance_private (group);

  g_return_val_if_fail (VALENT_IS_DEVICE_PREFERENCES_GROUP (group), NULL);

  if (priv->context == NULL)
    {
      g_autoptr (ValentContext) context = NULL;

      context = valent_context_new (NULL, "device", "default");
      priv->context = valent_context_get_plugin_context (context, priv->plugin_info);
    }

  return priv->context;
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

  if (priv->settings == NULL)
    {
      ValentContext *context = NULL;

      context = valent_device_preferences_group_get_context (group);
      priv->settings = valent_context_get_plugin_settings (context,
                                                           priv->plugin_info,
                                                           "X-DevicePluginSettings");
    }

  return priv->settings;
}

