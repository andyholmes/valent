// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-component"

#include "config.h"

#include <gio/gio.h>
#include <libpeas/peas.h>

#include "valent-component.h"
#include "valent-component-private.h"
#include "valent-debug.h"
#include "valent-global.h"
#include "valent-object.h"


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
  PeasEngine    *engine;
  ValentContext *context;
  char          *plugin_domain;
  char          *plugin_priority;
  GType          plugin_type;
  GHashTable    *plugins;
  GObject       *preferred;
} ValentComponentPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (ValentComponent, valent_component, VALENT_TYPE_OBJECT);


/**
 * ValentComponentClass:
 * @bind_extension: the virtual function pointer for bind_extension()
 * @unbind_extension: the virtual function pointer for unbind_extension()
 *
 * The virtual function table for #ValentComponent.
 */

enum {
  PROP_0,
  PROP_PLUGIN_DOMAIN,
  PROP_PLUGIN_PRIORITY,
  PROP_PLUGIN_TYPE,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };


static void
component_plugin_free (gpointer data)
{
  ValentPlugin *plugin = data;

  if (plugin->extension != NULL)
    {
      ValentComponentClass *klass = VALENT_COMPONENT_GET_CLASS (plugin->parent);

      if (klass->unbind_extension != NULL)
        klass->unbind_extension (plugin->parent, plugin->extension);
    }

  g_clear_pointer (&plugin, valent_plugin_free);
}


static gint64
_peas_plugin_info_get_priority (PeasPluginInfo *info,
                                const char     *key)
{
  const char *priority_str = NULL;
  gint64 priority = 0;

  if (info != NULL && key != NULL)
    priority_str = peas_plugin_info_get_external_data (info, key);

  if (priority_str != NULL)
    priority = g_ascii_strtoll (priority_str, NULL, 10);

  return priority;
}

static void
valent_component_update_preferred (ValentComponent *self)
{
  ValentComponentPrivate *priv = valent_component_get_instance_private (self);
  GHashTableIter iter;
  PeasPluginInfo *info;
  ValentPlugin *plugin;
  GObject *extension = NULL;
  gint64 extension_priority = 0;

  g_assert (VALENT_IS_COMPONENT (self));

  g_hash_table_iter_init (&iter, priv->plugins);

  while (g_hash_table_iter_next (&iter, (void **)&info, (void **)&plugin))
    {
      gint64 priority;

      if (plugin->extension == NULL)
        continue;

      priority = _peas_plugin_info_get_priority (info, priv->plugin_priority);

      if (extension == NULL || priority < extension_priority)
        {
          extension = plugin->extension;
          extension_priority = priority;
        }
    }

  if (priv->preferred != extension)
    {
      priv->preferred = extension;
      VALENT_COMPONENT_GET_CLASS (self)->bind_preferred (self, priv->preferred);
    }
}


/*
 * GSettings Handlers
 */
static void
g_async_initable_init_async_cb (GObject      *object,
                                GAsyncResult *result,
                                gpointer      user_data)
{
  GAsyncInitable *initable = G_ASYNC_INITABLE (object);
  g_autoptr (GError) error = NULL;

  VALENT_ENTRY;

  if (!g_async_initable_init_finish (initable, result, &error) &&
      !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
      g_warning ("%s initialization failed: %s",
                 G_OBJECT_TYPE_NAME (initable),
                 error->message);
    }

  VALENT_EXIT;
}

static void
valent_component_enable_plugin (ValentComponent *self,
                                ValentPlugin    *plugin)
{
  ValentComponentPrivate *priv = valent_component_get_instance_private (self);

  VALENT_ENTRY;

  g_assert (VALENT_IS_COMPONENT (self));
  g_assert (plugin != NULL);

  plugin->extension = peas_engine_create_extension (priv->engine,
                                                    plugin->info,
                                                    priv->plugin_type,
                                                    NULL);
  g_return_if_fail (G_IS_OBJECT (plugin->extension));

  VALENT_COMPONENT_GET_CLASS (self)->bind_extension (self, plugin->extension);
  valent_component_update_preferred (self);

  /* If the extension requires initialization, use a chained cancellable in case
   * the plugin is unloaded or the component is destroyed. */
  if (G_IS_ASYNC_INITABLE (plugin->extension))
    {
      GAsyncInitable *initable = G_ASYNC_INITABLE (plugin->extension);
      g_autoptr (GCancellable) destroy = NULL;

      plugin->cancellable = g_cancellable_new ();
      destroy = valent_object_chain_cancellable (VALENT_OBJECT (self),
                                                 plugin->cancellable);

      g_async_initable_init_async (initable,
                                   G_PRIORITY_DEFAULT,
                                   destroy,
                                   g_async_initable_init_async_cb,
                                   NULL);
    }
  else if (G_IS_INITABLE (plugin->extension))
    {
      GInitable *initable = G_INITABLE (plugin->extension);
      g_autoptr (GCancellable) destroy = NULL;
      g_autoptr (GError) error = NULL;

      plugin->cancellable = g_cancellable_new ();
      destroy = valent_object_chain_cancellable (VALENT_OBJECT (self),
                                                 plugin->cancellable);

      if (!g_initable_init (initable, destroy, &error) &&
          !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          g_warning ("%s initialization failed: %s",
                     G_OBJECT_TYPE_NAME (initable),
                     error->message);
        }
    }

  VALENT_EXIT;
}

static void
valent_component_disable_plugin (ValentComponent *self,
                                 ValentPlugin    *plugin)
{
  ValentComponentPrivate *priv = valent_component_get_instance_private (self);
  g_autoptr (GObject) extension = NULL;

  g_assert (VALENT_IS_COMPONENT (self));
  g_assert (plugin != NULL);

  /* Ensure any in-progress initialization is cancelled */
  g_cancellable_cancel (plugin->cancellable);
  g_clear_object (&plugin->cancellable);

  /* Steal the object and reset the preferred adapter */
  extension = g_steal_pointer (&plugin->extension);
  g_return_if_fail (G_IS_OBJECT (extension));

  if (priv->preferred == extension)
    valent_component_update_preferred (self);

  VALENT_COMPONENT_GET_CLASS (self)->unbind_extension (self, extension);
}

static void
on_plugin_enabled_changed (ValentPlugin *plugin)
{
  g_assert (plugin != NULL);
  g_assert (VALENT_IS_COMPONENT (plugin->parent));

  if (valent_plugin_get_enabled (plugin))
    valent_component_enable_plugin (plugin->parent, plugin);
  else
    valent_component_disable_plugin (plugin->parent, plugin);
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
  ValentPlugin *plugin;

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

  plugin = valent_plugin_new (self, priv->context, info,
                              G_CALLBACK (on_plugin_enabled_changed));
  g_hash_table_insert (priv->plugins, info, plugin);

  if (valent_plugin_get_enabled (plugin))
    valent_component_enable_plugin (self, plugin);

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

/* LCOV_EXCL_START */
static void
valent_component_real_bind_preferred (ValentComponent *component,
                                      GObject         *extension)
{
  g_assert (VALENT_IS_COMPONENT (component));
  g_assert (G_IS_OBJECT (extension));
}

static void
valent_component_real_bind_extension (ValentComponent *component,
                                      GObject         *extension)
{
  g_assert (VALENT_IS_COMPONENT (component));
  g_assert (G_IS_OBJECT (extension));
}

static void
valent_component_real_unbind_extension (ValentComponent *component,
                                        GObject         *extension)
{
  g_assert (VALENT_IS_COMPONENT (component));
  g_assert (G_IS_OBJECT (extension));
}
/* LCOV_EXCL_STOP */

/*< private >
 * valent_component_get_preferred:
 * @self: a #ValentComponent
 *
 * Get the extension with the highest priority for @self.
 *
 * The default value for extensions is `0`; the lower the value the higher the
 * priority.
 *
 * Returns: (transfer none) (nullable): a `GObject`
 */
GObject *
valent_component_get_preferred (ValentComponent *self)
{
  ValentComponentPrivate *priv = valent_component_get_instance_private (self);

  g_assert (VALENT_IS_COMPONENT (self));

  return priv->preferred;
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

  g_assert (priv->plugin_domain != NULL);
  g_assert (priv->plugin_type != G_TYPE_NONE);

  priv->context = valent_context_new (NULL, priv->plugin_domain, NULL);

  /* Setup PeasEngine */
  plugins = peas_engine_get_plugin_list (priv->engine);

  for (const GList *iter = plugins; iter; iter = iter->next)
    {
      if (peas_plugin_info_is_loaded (iter->data))
        on_load_plugin (priv->engine, iter->data, self);
    }

  g_signal_connect_object (priv->engine,
                           "load-plugin",
                           G_CALLBACK (on_load_plugin),
                           self,
                           G_CONNECT_AFTER);

  g_signal_connect_object (priv->engine,
                           "unload-plugin",
                           G_CALLBACK (on_unload_plugin),
                           self,
                           0);

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

  g_clear_pointer (&priv->plugin_domain, g_free);
  g_clear_pointer (&priv->plugin_priority, g_free);
  g_clear_pointer (&priv->plugins, g_hash_table_unref);
  g_clear_object (&priv->context);

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
    case PROP_PLUGIN_DOMAIN:
      g_value_set_string (value, priv->plugin_domain);
      break;

    case PROP_PLUGIN_PRIORITY:
      g_value_set_string (value, priv->plugin_priority);
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
    case PROP_PLUGIN_DOMAIN:
      priv->plugin_domain = g_value_dup_string (value);
      break;

    case PROP_PLUGIN_PRIORITY:
      priv->plugin_priority = g_value_dup_string (value);
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

  klass->bind_extension = valent_component_real_bind_extension;
  klass->unbind_extension = valent_component_real_unbind_extension;
  klass->bind_preferred = valent_component_real_bind_preferred;

  /**
   * ValentComponent:plugin-context:
   *
   * The domain of the component.
   *
   * This is a `GSettings` safe string such as "contacts" or "media", used to
   * structure settings and files of components and their extensions.
   *
   * Since: 1.0
   */
  properties [PROP_PLUGIN_DOMAIN] =
    g_param_spec_string ("plugin-domain", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentComponent:plugin-priority:
   *
   * The priority key for the component.
   *
   * This is the name of a key in the `.plugin` file used to determine the
   * preferred implementation.
   *
   * Since: 1.0
   */
  properties [PROP_PLUGIN_PRIORITY] =
    g_param_spec_string ("plugin-priority", NULL, NULL,
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
    g_param_spec_gtype ("plugin-type", NULL, NULL,
                        G_TYPE_NONE,
                        (G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
valent_component_init (ValentComponent *self)
{
  ValentComponentPrivate *priv = valent_component_get_instance_private (self);

  priv->engine = valent_get_plugin_engine ();
  priv->plugins = g_hash_table_new_full (NULL, NULL, NULL, component_plugin_free);
}

