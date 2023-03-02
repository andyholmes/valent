// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-device-preferences"

#include "config.h"

#include <gio/gio.h>
#include <gtk/gtk.h>
#include <adwaita.h>
#include <libvalent-core.h>
#include <libvalent-device.h>

#include "valent-device-preferences-page.h"


/**
 * ValentDevicePreferencesPage:
 *
 * An abstract base class for device plugin preferences.
 *
 * #ValentDevicePreferencesPage is an base class for [class@Valent.DevicePlugin]
 * implementations that want to provide a preferences page.
 *
 * Since: 1.0
 */

typedef struct
{
  ValentContext  *context;
  PeasPluginInfo *plugin_info;
  GSettings      *settings;
} ValentDevicePreferencesPagePrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (ValentDevicePreferencesPage, valent_device_preferences_page, ADW_TYPE_PREFERENCES_PAGE)

enum {
  PROP_0,
  PROP_CONTEXT,
  PROP_PLUGIN_INFO,
  PROP_SETTINGS,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

/**
 * ValentDevicePreferencesPageClass:
 *
 * The virtual function table for #ValentDevicePreferencesPage.
 *
 * Since: 1.0
 */


/*
 * GObject
 */
static void
valent_device_preferences_page_finalize (GObject *object)
{
  ValentDevicePreferencesPage *self = VALENT_DEVICE_PREFERENCES_PAGE (object);
  ValentDevicePreferencesPagePrivate *priv = valent_device_preferences_page_get_instance_private (self);

  g_clear_object (&priv->context);
  g_clear_object (&priv->settings);

  G_OBJECT_CLASS (valent_device_preferences_page_parent_class)->finalize (object);
}

static void
valent_device_preferences_page_get_property (GObject    *object,
                                             guint       prop_id,
                                             GValue     *value,
                                             GParamSpec *pspec)
{
  ValentDevicePreferencesPage *self = VALENT_DEVICE_PREFERENCES_PAGE (object);
  ValentDevicePreferencesPagePrivate *priv = valent_device_preferences_page_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      g_value_set_object (value, valent_device_preferences_page_get_context (self));
      break;

    case PROP_PLUGIN_INFO:
      g_value_set_boxed (value, priv->plugin_info);
      break;

    case PROP_SETTINGS:
      g_value_set_object (value, valent_device_preferences_page_get_settings (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_device_preferences_page_set_property (GObject      *object,
                                             guint         prop_id,
                                             const GValue *value,
                                             GParamSpec   *pspec)
{
  ValentDevicePreferencesPage *self = VALENT_DEVICE_PREFERENCES_PAGE (object);
  ValentDevicePreferencesPagePrivate *priv = valent_device_preferences_page_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      priv->context = g_value_dup_object (value);
      break;

    case PROP_PLUGIN_INFO:
      priv->plugin_info = g_value_get_boxed (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_device_preferences_page_class_init (ValentDevicePreferencesPageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = valent_device_preferences_page_finalize;
  object_class->get_property = valent_device_preferences_page_get_property;
  object_class->set_property = valent_device_preferences_page_set_property;

  /**
   * ValentDevicePreferencesPage:context: (getter get_context)
   *
   * The [class@Valent.Context] for the [class@Valent.DevicePlugin].
   *
   * Since: 1.0
   */
  properties [PROP_CONTEXT] =
    g_param_spec_object ("context", NULL, NULL,
                         VALENT_TYPE_CONTEXT,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentDevicePreferencesPage:plugin-info:
   *
   * The [struct@Peas.PluginInfo] describing this plugin.
   *
   * Since: 1.0
   */
  properties [PROP_PLUGIN_INFO] =
    g_param_spec_boxed ("plugin-info", NULL, NULL,
                        PEAS_TYPE_PLUGIN_INFO,
                        (G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  /**
   * ValentDevicePreferencesPage:settings: (getter get_settings)
   *
   * The [class@Gio.Settings] for the [class@Valent.DevicePlugin].
   *
   * Since: 1.0
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
valent_device_preferences_page_init (ValentDevicePreferencesPage *self)
{
}

/**
 * valent_device_preferences_page_get_context:
 * @page: a #ValentDevicePreferencesPage
 *
 * Get the [class@Valent.Context] for the [class@Valent.DevicePlugin].
 *
 * Returns: (transfer none) (nullable): a #ValentContext
 *
 * Since: 1.0
 */
ValentContext *
valent_device_preferences_page_get_context (ValentDevicePreferencesPage *page)
{
  ValentDevicePreferencesPagePrivate *priv = valent_device_preferences_page_get_instance_private (page);

  if (priv->context == NULL)
    {
      g_autoptr (ValentContext) context = NULL;

      context = valent_context_new (NULL, "device", "default");
      priv->context = valent_context_get_plugin_context (context, priv->plugin_info);
    }

  return priv->context;
}

/**
 * valent_device_preferences_page_get_settings:
 * @page: a #ValentDevicePreferencesPage
 *
 * Get the [class@Gio.Settings] for the [class@Valent.DevicePlugin].
 *
 * Returns: (transfer none) (nullable): a `GSettings`
 *
 * Since: 1.0
 */
GSettings *
valent_device_preferences_page_get_settings (ValentDevicePreferencesPage *page)
{
  ValentDevicePreferencesPagePrivate *priv = valent_device_preferences_page_get_instance_private (page);

  /* Setup GSettings */
  if (priv->settings == NULL)
    {
      ValentContext *context = NULL;

      context = valent_device_preferences_page_get_context (page);
      priv->settings = valent_context_get_plugin_settings (context,
                                                           priv->plugin_info,
                                                           "X-DevicePluginSettings");
    }

  return priv->settings;
}

