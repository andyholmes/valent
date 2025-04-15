// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-device-preferences"

#include "config.h"

#include <gio/gio.h>
#include <gtk/gtk.h>
#include <adwaita.h>
#include <valent.h>

#include "valent-preferences-page.h"

typedef struct
{
  ValentContext  *context;
  GHashTable     *settings;
} ValentPreferencesPagePrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (ValentPreferencesPage, valent_preferences_page, ADW_TYPE_PREFERENCES_PAGE)

typedef enum {
  PROP_CONTEXT = 1,
} ValentPreferencesPageProperty;

static GParamSpec *properties[PROP_CONTEXT + 1] = { NULL, };

/*
 * GObject
 */
static void
valent_preferences_page_finalize (GObject *object)
{
  ValentPreferencesPage *self = VALENT_PREFERENCES_PAGE (object);
  ValentPreferencesPagePrivate *priv = valent_preferences_page_get_instance_private (self);

  g_clear_object (&priv->context);
  g_clear_pointer (&priv->settings, g_hash_table_unref);

  G_OBJECT_CLASS (valent_preferences_page_parent_class)->finalize (object);
}

static void
valent_preferences_page_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  ValentPreferencesPage *self = VALENT_PREFERENCES_PAGE (object);
  ValentPreferencesPagePrivate *priv = valent_preferences_page_get_instance_private (self);

  switch ((ValentPreferencesPageProperty)prop_id)
    {
    case PROP_CONTEXT:
      g_value_set_object (value, priv->context);
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

  switch ((ValentPreferencesPageProperty)prop_id)
    {
    case PROP_CONTEXT:
      g_set_object (&priv->context, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_preferences_page_class_init (ValentPreferencesPageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = valent_preferences_page_finalize;
  object_class->get_property = valent_preferences_page_get_property;
  object_class->set_property = valent_preferences_page_set_property;

  properties [PROP_CONTEXT] =
    g_param_spec_object ("context", NULL, NULL,
                         VALENT_TYPE_CONTEXT,
                         (G_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, G_N_ELEMENTS (properties), properties);
}

static void
valent_preferences_page_init (ValentPreferencesPage *self)
{
  ValentPreferencesPagePrivate *priv = valent_preferences_page_get_instance_private (self);

  priv->settings = g_hash_table_new_full (g_str_hash, g_str_equal,
                                          g_free, g_object_unref);
}

ValentContext *
valent_preferences_page_get_context (ValentPreferencesPage *page)
{
  ValentPreferencesPagePrivate *priv = valent_preferences_page_get_instance_private (page);

  g_return_val_if_fail (VALENT_IS_PREFERENCES_PAGE (page), NULL);

  return priv->context;
}

GSettings *
valent_preferences_page_get_settings (ValentPreferencesPage *page,
                                      const char            *name)
{
  ValentPreferencesPagePrivate *priv = valent_preferences_page_get_instance_private (page);
  GSettings *settings = NULL;

  g_return_val_if_fail (VALENT_IS_PREFERENCES_PAGE (page), NULL);
  g_return_val_if_fail (name != NULL && *name != '\0', NULL);

  settings = g_hash_table_lookup (priv->settings, name);
  if (settings == NULL)
    {
      g_autoptr (ValentContext) plugin_context = NULL;
      PeasEngine *engine = valent_get_plugin_engine ();
      PeasPluginInfo *plugin_info = NULL;

      plugin_context = g_object_new (VALENT_TYPE_CONTEXT,
                                     "parent", priv->context,
                                     "domain", "plugin",
                                     "id",     name,
                                     NULL);
      plugin_info = peas_engine_get_plugin_info (engine, name);
      settings = valent_context_get_plugin_settings (plugin_context,
                                                     plugin_info,
                                                     "X-DevicePluginSettings");
      g_hash_table_replace (priv->settings, g_strdup (name), settings);
    }

  g_return_val_if_fail (G_IS_SETTINGS (settings), NULL);

  return settings;
}

