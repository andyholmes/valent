// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-component"

#include "config.h"

#include <gio/gio.h>
#include <libpeas/peas.h>

#include "valent-component.h"
#include "valent-debug.h"
#include "valent-utils.h"


/**
 * ValentComponent:
 *
 * An abstract base class for components.
 *
 * #ValentComponent is a base class for session and system components, such as
 * the clipboard or volume control. Each component is typically used in a
 * singleton pattern, backed by one or more extensions.
 *
 * Since: 1.0
 */

typedef struct
{
  PeasEngine *engine;
  char       *plugin_context;
  GType       plugin_type;
  GHashTable *plugins;
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
  PeasExtension   *extension;
  GSettings       *settings;
} ComponentPlugin;

static void
component_plugin_free (gpointer data)
{
  ComponentPlugin *plugin = data;

  if (plugin->extension != NULL)
    {
      g_signal_emit (G_OBJECT (plugin->component),
                     signals [EXTENSION_REMOVED], 0,
                     plugin->extension);
      g_clear_object (&plugin->extension);
    }

  g_clear_object (&plugin->settings);
  g_clear_pointer (&plugin, g_free);
}


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
valent_component_enable_extension (ValentComponent *self,
                                   ComponentPlugin *plugin)
{
  ValentComponentPrivate *priv = valent_component_get_instance_private (self);

  g_assert (VALENT_IS_COMPONENT (self));
  g_assert (plugin != NULL);

  plugin->extension = peas_engine_create_extension (priv->engine,
                                                    plugin->info,
                                                    priv->plugin_type,
                                                    NULL);
  g_return_if_fail (PEAS_IS_EXTENSION (plugin->extension));

  g_signal_emit (G_OBJECT (self),
                 signals [EXTENSION_ADDED], 0,
                 plugin->extension);
}

static void
valent_component_disable_extension (ValentComponent *self,
                                    ComponentPlugin *plugin)
{
  g_assert (VALENT_IS_COMPONENT (self));
  g_assert (plugin != NULL);
  g_return_if_fail (PEAS_IS_EXTENSION (plugin->extension));

  g_signal_emit (G_OBJECT (self),
                 signals [EXTENSION_REMOVED], 0,
                 plugin->extension);
  g_clear_object (&plugin->extension);
}

static void
on_enabled_changed (GSettings       *settings,
                    const char      *key,
                    ComponentPlugin *plugin)
{
  ValentComponent *self = VALENT_COMPONENT (plugin->component);

  g_assert (G_IS_SETTINGS (settings));
  g_assert (VALENT_IS_COMPONENT (self));

  if (g_settings_get_boolean (settings, key))
    valent_component_enable_extension (self, plugin);
  else
    valent_component_disable_extension (self, plugin);
}

/*
 * PeasEngine Callbacks
 */
static void
on_load_plugin (PeasEngine      *engine,
                PeasPluginInfo  *info,
                ValentComponent *self)
{
  ValentComponentPrivate *priv = valent_component_get_instance_private (self);
  ComponentPlugin *plugin;
  const char *module;

  VALENT_ENTRY;

  g_assert (PEAS_IS_ENGINE (engine));
  g_assert (info != NULL);
  g_assert (VALENT_IS_COMPONENT (self));

  /* We're only interested in one GType */
  if (!peas_engine_provides_extension (engine, info, priv->plugin_type))
    VALENT_EXIT;

  VALENT_NOTE ("%s: %s",
               g_type_name (priv->plugin_type),
               peas_plugin_info_get_module_name (info));

  module = peas_plugin_info_get_module_name (info);

  plugin = g_new0 (ComponentPlugin, 1);
  plugin->component = self;
  plugin->info = info;
  plugin->settings = valent_component_new_settings (priv->plugin_context,
                                                    module);
  g_hash_table_insert (priv->plugins, info, plugin);

  /* The PeasExtension is created and destroyed based on the enabled state */
  g_signal_connect (plugin->settings,
                    "changed::enabled",
                    G_CALLBACK (on_enabled_changed),
                    plugin);

  if (g_settings_get_boolean (plugin->settings, "enabled"))
    valent_component_enable_extension (self, plugin);

  VALENT_EXIT;
}

static void
on_unload_plugin (PeasEngine      *engine,
                  PeasPluginInfo  *info,
                  ValentComponent *self)
{
  ValentComponentPrivate *priv = valent_component_get_instance_private (self);

  VALENT_ENTRY;

  g_assert (PEAS_IS_ENGINE (engine));
  g_assert (info != NULL);
  g_assert (VALENT_IS_COMPONENT (self));

  /* We're only interested in one GType */
  if (!peas_engine_provides_extension (engine, info, priv->plugin_type))
    VALENT_EXIT;

  VALENT_NOTE ("%s: %s",
               g_type_name (priv->plugin_type),
               peas_plugin_info_get_module_name (info));

  g_hash_table_remove (priv->plugins, info);

  VALENT_EXIT;
}


/*
 * GObject
 */
static void
valent_component_constructed (GObject *object)
{
  ValentComponent *self = VALENT_COMPONENT (object);
  ValentComponentPrivate *priv = valent_component_get_instance_private (self);
  const GList *plugins = NULL;

  g_assert (priv->plugin_context != NULL);
  g_assert (priv->plugin_type != G_TYPE_NONE);

  /* Setup PeasEngine */
  plugins = peas_engine_get_plugin_list (priv->engine);

  for (const GList *iter = plugins; iter; iter = iter->next)
    {
      if (peas_plugin_info_is_loaded (iter->data))
        on_load_plugin (priv->engine, iter->data, self);
    }

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

  g_signal_handlers_disconnect_by_func (priv->engine, on_load_plugin, self);
  g_signal_handlers_disconnect_by_func (priv->engine, on_unload_plugin, self);
  g_hash_table_remove_all (priv->plugins);

  G_OBJECT_CLASS (valent_component_parent_class)->dispose (object);
}

static void
valent_component_finalize (GObject *object)
{
  ValentComponent *self = VALENT_COMPONENT (object);
  ValentComponentPrivate *priv = valent_component_get_instance_private (self);

  g_clear_pointer (&priv->plugin_context, g_free);
  g_clear_pointer (&priv->plugins, g_hash_table_unref);

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
   * The domain of the component.
   *
   * This is a #GSettings safe string such as "contacts" or "media", used to
   * structure settings and files of components and their extensions.
   *
   * Since: 1.0
   */
  properties [PROP_PLUGIN_CONTEXT] =
    g_param_spec_string ("plugin-context",
                         "Plugin Context",
                         "The context of the component",
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentComponent:plugin-type:
   *
   * The extension point [alias@GLib.Type].
   *
   * Since: 1.0
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
   * Emitted when a [alias@Peas.Extension] has been enabled.
   *
   * Implementations of [class@Valent.Component] must chain-up if they override
   * [vfunc@Valent.Component.extension_added].
   *
   * Since: 1.0
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
   * Emitted when a [alias@Peas.Extension] has been disabled.
   *
   * Implementations of [class@Valent.Component] must chain-up if they override
   * [vfunc@Valent.Component.extension_removed].
   *
   * Since: 1.0
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
  priv->plugins = g_hash_table_new_full (NULL, NULL, NULL, component_plugin_free);
}

/**
 * valent_component_get_priority_provider:
 * @component: a #ValentComponent
 * @key: The priority key in the plugin info
 *
 * Get the extension with the highest priority.
 *
 * The default value for extensions is `0`; the lower the value the higher the
 * priority.
 *
 * Returns: (transfer none) (nullable): a #PeasExtension
 *
 * Since: 1.0
 */
PeasExtension *
valent_component_get_priority_provider (ValentComponent *component,
                                        const char      *key)
{
  ValentComponentPrivate *priv = valent_component_get_instance_private (component);
  GHashTableIter iter;
  PeasPluginInfo *info;
  ComponentPlugin *plugin;
  PeasExtension *extension = NULL;
  gint64 priority = 0;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_COMPONENT (component), NULL);
  g_return_val_if_fail (key != NULL && *key != '\0', NULL);

  g_hash_table_iter_init (&iter, priv->plugins);

  while (g_hash_table_iter_next (&iter, (void **)&info, (void **)&plugin))
    {
      gint64 curr_priority;

      if (plugin->extension == NULL)
        continue;

      curr_priority = get_extension_priority (info, key);

      if (extension == NULL || curr_priority < priority)
        {
          priority = curr_priority;
          extension = plugin->extension;
        }
    }

  VALENT_RETURN (extension);
}

/**
 * valent_component_new_settings:
 * @context: a #ValentDevice ID
 * @module_name: a #PeasPluginInfo module name
 *
 * Create a [class@Gio.Settings] for an extension.
 *
 * A convenience function to create a #GSettings object for a context and module
 * name.
 *
 * Returns: (transfer full): the new #GSettings object
 *
 * Since: 1.0
 */
GSettings *
valent_component_new_settings (const char *context,
                               const char *module_name)
{
  g_autofree char *path = NULL;

  g_return_val_if_fail (context != NULL, NULL);
  g_return_val_if_fail (module_name != NULL, NULL);

  path = g_strdup_printf ("/ca/andyholmes/valent/%s/plugin/%s/",
                          context, module_name);

  return g_settings_new_with_path ("ca.andyholmes.Valent.Plugin", path);
}

