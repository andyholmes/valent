// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-application-plugin"

#include "config.h"

#include <gio/gio.h>
#include <libpeas/peas.h>

#include "valent-application-plugin.h"
#include "valent-object.h"


/**
 * ValentApplicationPlugin:
 *
 * An abstract base-class for application plugins.
 *
 * #ValentApplicationPlugin is a base-class for plugins that operate in the
 * scope of the application. This generally means integrating with the
 * desktop in a way that is unrelated to devices.
 *
 * # `.plugin` File
 *
 * Application plugins have no special fields in the `.plugin` file.
 */

typedef struct
{
  PeasPluginInfo *plugin_info;
  GApplication   *application;
} ValentApplicationPluginPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (ValentApplicationPlugin, valent_application_plugin, VALENT_TYPE_OBJECT)

/**
 * ValentApplicationPluginClass:
 * @enable: the virtual function pointer for valent_application_plugin_enable()
 * @disable: the virtual function pointer for valent_application_plugin_disable()
 *
 * The virtual function table for #ValentApplicationPlugin.
 */

enum {
  PROP_0,
  PROP_APPLICATION,
  PROP_PLUGIN_INFO,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };


/* LCOV_EXCL_START */
static void
valent_application_plugin_real_disable (ValentApplicationPlugin *plugin)
{
}

static void
valent_application_plugin_real_enable (ValentApplicationPlugin *plugin)
{
}
/* LCOV_EXCL_STOP */


/*
 * GObject
 */
static void
valent_application_plugin_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  ValentApplicationPlugin *self = VALENT_APPLICATION_PLUGIN (object);
  ValentApplicationPluginPrivate *priv = valent_application_plugin_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_APPLICATION:
      g_value_set_object (value, priv->application);
      break;

    case PROP_PLUGIN_INFO:
      g_value_set_boxed (value, priv->plugin_info);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_application_plugin_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  ValentApplicationPlugin *self = VALENT_APPLICATION_PLUGIN (object);
  ValentApplicationPluginPrivate *priv = valent_application_plugin_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_APPLICATION:
      priv->application = g_value_get_object (value);
      break;

    case PROP_PLUGIN_INFO:
      priv->plugin_info = g_value_get_boxed (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_application_plugin_class_init (ValentApplicationPluginClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = valent_application_plugin_get_property;
  object_class->set_property = valent_application_plugin_set_property;

  klass->disable = valent_application_plugin_real_disable;
  klass->enable = valent_application_plugin_real_enable;

  /**
   * ValentApplicationPlugin:application:
   *
   * The [class@Gio.Application] this plugin is bound to.
   *
   * Since: 1.0
   */
  properties [PROP_APPLICATION] =
    g_param_spec_object ("application",
                         "Application",
                         "The application this plugin is bound to",
                         G_TYPE_APPLICATION,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentApplicationPlugin:plugin-info:
   *
   * The [struct@Peas.PluginInfo] describing this plugin.
   *
   * Since: 1.0
   */
  properties [PROP_PLUGIN_INFO] =
    g_param_spec_boxed ("plugin-info",
                        "Plugin Info",
                        "The plugin info describing this plugin",
                        PEAS_TYPE_PLUGIN_INFO,
                        (G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
valent_application_plugin_init (ValentApplicationPlugin *adapter)
{
}

/**
 * valent_application_plugin_enable: (virtual enable)
 * @plugin: a #ValentApplicationPlugin
 *
 * This function is called after the extension is loaded and should prepare
 * anything the plugin needs to perform its function.
 *
 * Since: 1.0
 */
void
valent_application_plugin_enable (ValentApplicationPlugin *plugin)
{
  g_return_if_fail (VALENT_IS_APPLICATION_PLUGIN (plugin));

  VALENT_APPLICATION_PLUGIN_GET_CLASS (plugin)->enable (plugin);
}

/**
 * valent_application_plugin_disable: (virtual disable)
 * @plugin: a #ValentApplicationPlugin
 *
 * This function is called before the extension is unloaded and should clean up
 * any resources that were allocated in valent_application_plugin_enable().
 *
 * It is guaranteed that this function will be called if
 * valent_application_plugin_enable() has been called.
 *
 * Since: 1.0
 */
void
valent_application_plugin_disable (ValentApplicationPlugin *plugin)
{
  g_return_if_fail (VALENT_IS_APPLICATION_PLUGIN (plugin));

  VALENT_APPLICATION_PLUGIN_GET_CLASS (plugin)->disable (plugin);
}

