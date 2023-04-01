// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-preferences-page"

#include "config.h"

#include <adwaita.h>
#include <libvalent-core.h>

#include "valent-preferences-page.h"


/**
 * ValentPreferencesPage:
 *
 * An abstract base class for plugin preferences.
 *
 * #ValentPreferencesPage is a base class for plugins that want to provide a
 * preferences page. Unlike [class@Valent.DevicePreferencesGroup] the page should
 * configure all of the plugin's extension implementations, with the exception
 * of [class@Valent.DevicePlugin].
 *
 * Implementations of [class@Valent.DevicePlugin] should instead implement
 * [class@Valent.DevicePreferencesGroup], which will allow plugins to store
 * per-devices settings.
 *
 * Since: 1.0
 */

typedef struct
{
  PeasPluginInfo *plugin_info;
} ValentPreferencesPagePrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (ValentPreferencesPage, valent_preferences_page, ADW_TYPE_PREFERENCES_PAGE)

enum {
  PROP_0,
  PROP_PLUGIN_INFO,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

/**
 * ValentPreferencesPageClass:
 *
 * The virtual function table for #ValentPreferencesPage.
 *
 * Since: 1.0
 */


/*
 * GObject
 */
static void
valent_preferences_page_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  ValentPreferencesPage *self = VALENT_PREFERENCES_PAGE (object);
  ValentPreferencesPagePrivate *priv = valent_preferences_page_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_PLUGIN_INFO:
      g_value_set_boxed (value, priv->plugin_info);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_preferences_page_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  ValentPreferencesPage *self = VALENT_PREFERENCES_PAGE (object);
  ValentPreferencesPagePrivate *priv = valent_preferences_page_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_PLUGIN_INFO:
      priv->plugin_info = g_value_get_boxed (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_preferences_page_class_init (ValentPreferencesPageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = valent_preferences_page_get_property;
  object_class->set_property = valent_preferences_page_set_property;

  /**
   * ValentPreferencesPage:plugin-info:
   *
   * The [struct@Peas.PluginInfo] describing the plugin this page configures.
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

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
valent_preferences_page_init (ValentPreferencesPage *self)
{
}

