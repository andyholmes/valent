// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-component"

#include "config.h"

#include <gio/gio.h>
#include <libpeas/peas.h>

#include "valent-component.h"
#include "valent-utils.h"


/**
 * SECTION:valent-component
 * @short_description: Base class for components
 * @title: ValentComponent
 * @stability: Unstable
 * @include: libvalent-core.h
 *
 * #ValentComponent is a base class for session and system components, such as
 * the clipboard or volume control. Each #ValentComponent is typically a
 * singleton representing a unique resource, but may be backed by one or more
 * extensions implemented by #PeasExtensions.
 */

typedef struct
{
  PeasEngine *engine;
  char       *plugin_context;
  GType       plugin_type;

  GHashTable *extensions;
  GHashTable *settings;
} ValentComponentPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (ValentComponent, valent_component, G_TYPE_OBJECT);


/**
 * ValentComponentClass:
 * @extension_added: the class closure for the #ValentComponent::extension-added signal
 * @extension_removed: the class closure for the #ValentComponent::extension-removed signal
 *
 * The virtual function table for #ValentComponent.
 */

enum {
  PROP_0,
  PROP_PLUGIN_CONTEXT,
  PROP_PLUGIN_TYPE,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

enum {
  EXTENSION_ADDED,
  EXTENSION_REMOVED,
  N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0, };


typedef struct
{
  ValentComponent *component;
  PeasPluginInfo  *info;
} ComponentProviderInfo;


static gint64
get_extension_priority (PeasPluginInfo *info,
                       const char     *key)
{
  const char *priority_str = NULL;
  gint64 priority = 0;

  if (info != NULL)
    priority_str = peas_plugin_info_get_external_data (info, key);

  if (priority_str != NULL)
    priority = g_ascii_strtoll (priority_str, NULL, 10);

  return priority;
}


/*
 * Invoked when a source is enabled or disabled in settings.
 */
static void
valent_component_enable_extension (ValentComponent *component,
                                   PeasPluginInfo  *info)
{
  ValentComponentPrivate *priv = valent_component_get_instance_private (component);
  PeasExtension *extension;

  extension = peas_engine_create_extension (priv->engine,
                                            info,
                                            priv->plugin_type,
                                            NULL);

  if (extension != NULL)
    {
      g_hash_table_replace (priv->extensions, info, extension);
      g_signal_emit (G_OBJECT (component), signals [EXTENSION_ADDED], 0, extension);
    }
}

static void
valent_component_disable_extension (ValentComponent *component,
                                   PeasPluginInfo  *info)
{
  ValentComponentPrivate *priv = valent_component_get_instance_private (component);
  gpointer extension;

  if (g_hash_table_steal_extended (priv->extensions, info, NULL, &extension))
    {
      g_signal_emit (G_OBJECT (component), signals [EXTENSION_REMOVED], 0, extension);
      g_object_unref (extension);
    }
}

static void
on_enabled_changed (GSettings             *settings,
                    const char            *key,
                    ComponentProviderInfo *extension_info)
{
  ValentComponent *component = extension_info->component;
  PeasPluginInfo *info = extension_info->info;

  g_assert (G_IS_SETTINGS (settings));
  g_assert (VALENT_IS_COMPONENT (component));

  if (g_settings_get_boolean (settings, key))
    valent_component_enable_extension (component, info);
  else
    valent_component_disable_extension (component, info);
}

/*
 * PeasEngine Callbacks
 */
static void
on_load_plugin (PeasEngine      *engine,
                PeasPluginInfo  *info,
                ValentComponent *component)
{
  ValentComponentPrivate *priv = valent_component_get_instance_private (component);
  ComponentProviderInfo *extension_info;
  const char *module;
  g_autofree char *path = NULL;
  GSettings *settings;

  g_assert (PEAS_IS_ENGINE (engine));
  g_assert (info != NULL);
  g_assert (VALENT_IS_COMPONENT (component));

  /* This would generally only happen at startup */
  if G_UNLIKELY (!peas_plugin_info_is_loaded (info))
    return;

  /* We're only interested in one GType */
  if (!peas_engine_provides_extension (engine, info, priv->plugin_type))
    return;

  /* We create and destroy the #PeasExtension based on the enabled state to
   * ensure they aren't using any resources. */
  module = peas_plugin_info_get_module_name (info);
  path = g_strdup_printf ("/ca/andyholmes/valent/%s/%s/",
                          priv->plugin_context,
                          module);
  settings = g_settings_new_with_path ("ca.andyholmes.Valent.Plugin", path);
  g_hash_table_insert (priv->settings, info, settings);

  /* Watch for enabled/disabled */
  extension_info = g_new0 (ComponentProviderInfo, 1);
  extension_info->component = component;
  extension_info->info = info;

  g_signal_connect_data (settings,
                         "changed::enabled",
                         G_CALLBACK (on_enabled_changed),
                         extension_info,
                         (GClosureNotify)g_free,
                         0);

  if (g_settings_get_boolean (settings, "enabled"))
    valent_component_enable_extension (component, info);
}

static void
on_unload_plugin (PeasEngine      *engine,
                  PeasPluginInfo  *info,
                  ValentComponent *component)
{
  ValentComponentPrivate *priv = valent_component_get_instance_private (component);

  g_assert (PEAS_IS_ENGINE (engine));
  g_assert (info != NULL);
  g_assert (VALENT_IS_COMPONENT (component));

  /* We're only interested in one GType */
  if (!peas_engine_provides_extension (engine, info, priv->plugin_type))
    return;

  if (g_hash_table_remove (priv->settings, info))
    valent_component_disable_extension (component, info);
}


/*
 * GObject
 */
static void
valent_component_constructed (GObject *object)
{
  ValentComponent *self = VALENT_COMPONENT (object);
  ValentComponentPrivate *priv = valent_component_get_instance_private (self);
  const GList *plugins;

  g_assert (priv->plugin_context != NULL);
  g_assert (priv->plugin_type != G_TYPE_NONE);

  /* Setup known plugins */
  plugins = peas_engine_get_plugin_list (priv->engine);

  for (const GList *iter = plugins; iter; iter = iter->next)
    on_load_plugin (priv->engine, iter->data, self);

  /* Watch for new and removed plugins */
  g_signal_connect_after (priv->engine,
                          "load-plugin",
                          G_CALLBACK (on_load_plugin),
                          self);
  g_signal_connect (priv->engine,
                    "unload-plugin",
                    G_CALLBACK (on_unload_plugin),
                    self);

  G_OBJECT_CLASS (valent_component_parent_class)->constructed (object);
}

static void
valent_component_dispose (GObject *object)
{
  ValentComponent *self = VALENT_COMPONENT (object);
  ValentComponentPrivate *priv = valent_component_get_instance_private (self);
  GHashTableIter iter;
  gpointer info, extension;

  g_signal_handlers_disconnect_by_func (priv->engine, on_load_plugin, self);
  g_signal_handlers_disconnect_by_func (priv->engine, on_unload_plugin, self);

  g_hash_table_iter_init (&iter, priv->extensions);

  while (g_hash_table_iter_next (&iter, &info, &extension))
    {
      g_hash_table_remove (priv->settings, info);
      g_hash_table_iter_steal (&iter);

      g_signal_emit (G_OBJECT (self), signals [EXTENSION_REMOVED], 0, extension);
      g_object_unref (extension);
    }

  G_OBJECT_CLASS (valent_component_parent_class)->dispose (object);
}

static void
valent_component_finalize (GObject *object)
{
  ValentComponent *self = VALENT_COMPONENT (object);
  ValentComponentPrivate *priv = valent_component_get_instance_private (self);

  g_clear_pointer (&priv->plugin_context, g_free);
  g_clear_pointer (&priv->extensions, g_hash_table_unref);
  g_clear_pointer (&priv->settings, g_hash_table_unref);

  G_OBJECT_CLASS (valent_component_parent_class)->finalize (object);
}

static void
valent_component_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  ValentComponent *self = VALENT_COMPONENT (object);
  ValentComponentPrivate *priv = valent_component_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_PLUGIN_CONTEXT:
      g_value_set_string (value, priv->plugin_context);
      break;

    case PROP_PLUGIN_TYPE:
      g_value_set_gtype (value, priv->plugin_type);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_component_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  ValentComponent *self = VALENT_COMPONENT (object);
  ValentComponentPrivate *priv = valent_component_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_PLUGIN_CONTEXT:
      priv->plugin_context = g_value_dup_string (value);
      break;

    case PROP_PLUGIN_TYPE:
      priv->plugin_type = g_value_get_gtype (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_component_class_init (ValentComponentClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = valent_component_constructed;
  object_class->dispose = valent_component_dispose;
  object_class->finalize = valent_component_finalize;
  object_class->get_property = valent_component_get_property;
  object_class->set_property = valent_component_set_property;

  /**
   * ValentComponent:plugin-context:
   *
   * The context for the component. This is a #GSettings safe string such as
   * "contacts" or "media", used to distinguish the settings and configuration
   * files of pluggable extensions.
   */
  properties [PROP_PLUGIN_CONTEXT] =
    g_param_spec_string ("plugin-context",
                         "plugin-context",
                         "The context of the component",
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentComponent:plugin-type:
   *
   * The #GType of the extension this component aggregates.
   */
  properties [PROP_PLUGIN_TYPE] =
    g_param_spec_gtype ("plugin-type",
                        "Plugin Type",
                        "The GType of the component",
                        G_TYPE_NONE,
                        (G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);

  /**
   * ValentComponent::extension-added:
   * @component: an #ValentComponent
   * @extension: an #PeasExtension
   *
   * The "extension-added" signal is emitted when @component has enabled a
   * supported extension.
   *
   * Subclasses of #ValentComponent must chain-up if they override the
   * #ValentComponentClass.extension_added vfunc.
   */
  signals [EXTENSION_ADDED] =
    g_signal_new ("extension-added",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ValentComponentClass, extension_added),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, PEAS_TYPE_EXTENSION);
  g_signal_set_va_marshaller (signals [EXTENSION_ADDED],
                              G_TYPE_FROM_CLASS (klass),
                              g_cclosure_marshal_VOID__OBJECTv);

  /**
   * ValentComponent::extension-removed:
   * @component: an #ValentComponent
   * @extension: an #PeasExtension
   *
   * The "extension-removed" signal is emitted when a extension has discovered a
   * extension is no longer available.
   *
   * Subclasses of #ValentComponent must chain-up if they override the
   * #ValentComponentClass.extension_removed vfunc.
   */
  signals [EXTENSION_REMOVED] =
    g_signal_new ("extension-removed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ValentComponentClass, extension_removed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, PEAS_TYPE_EXTENSION);
  g_signal_set_va_marshaller (signals [EXTENSION_REMOVED],
                              G_TYPE_FROM_CLASS (klass),
                              g_cclosure_marshal_VOID__OBJECTv);
}

static void
valent_component_init (ValentComponent *self)
{
  ValentComponentPrivate *priv = valent_component_get_instance_private (self);

  priv->engine = valent_get_engine ();
  priv->plugin_type = G_TYPE_NONE;
  priv->extensions = g_hash_table_new_full (NULL, NULL, NULL, g_object_unref);
  priv->settings = g_hash_table_new_full (NULL, NULL, NULL, g_object_unref);
}

/**
 * valent_component_get_priority_provider:
 * @component: a #ValentComponent
 * @key: The priority key in the plugin info
 *
 * Get the extension with the highest priority for @component.
 *
 * Returns: (transfer none) (nullable): a #PeasExtension
 */
PeasExtension *
valent_component_get_priority_provider (ValentComponent *component,
                                        const char      *key)
{
  ValentComponentPrivate *priv = valent_component_get_instance_private (component);
  GHashTableIter iter;
  gpointer info, value;
  PeasExtension *extension = NULL;
  gint64 priority = 1000;

  g_return_val_if_fail (VALENT_IS_COMPONENT (component), NULL);

  g_hash_table_iter_init (&iter, priv->extensions);

  while (g_hash_table_iter_next (&iter, &info, &value))
    {
      gint64 curr_priority;

      curr_priority = get_extension_priority (info, key);

      if (curr_priority < priority)
        {
          priority = curr_priority;
          extension = value;
        }
    }

  return extension;
}

/**
 * valent_component_get_extensions:
 * @component: a #ValentComponent
 *
 * Get a list of the currently loaded extensions.
 *
 * Returns: (transfer container) (element-type Peas.Extension): a #GPtrArray
 */
GPtrArray *
valent_component_get_extensions (ValentComponent *component)
{
  ValentComponentPrivate *priv = valent_component_get_instance_private (component);
  GPtrArray *extensions;
  GHashTableIter iter;
  gpointer value;

  g_return_val_if_fail (VALENT_IS_COMPONENT (component), NULL);

  extensions = g_ptr_array_new_with_free_func (g_object_unref);
  g_hash_table_iter_init (&iter, priv->extensions);

  while (g_hash_table_iter_next (&iter, NULL, &value))
    g_ptr_array_add (extensions, g_object_ref (value));

  return extensions;
}

