// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-context"

#include "config.h"

#include <gio/gio.h>
#include <libpeas.h>

#include "valent-context.h"
#include "valent-debug.h"
#include "valent-macros.h"
#include "valent-object.h"


/**
 * ValentContext:
 *
 * A class representing a abstract data context.
 *
 * `ValentContext` is an abstraction of a data context, with a loose hierarchy
 * expressed as a virtual path (e.g. `device/0123456789abcdef/plugin/battery`).
 * It can be used to coordinate persistent data of various types by mapping onto
 * existing hierarchies like [class@Gio.Settings] (i.e. relocatable schemas),
 * on-disk caches and configuration files (i.e. XDG data dirs) and user files
 * (i.e. XDG user dirs).
 *
 * Since: 1.0
 */

struct _ValentContext
{
  ValentObject   parent_instance;

  ValentContext *parent;
  char          *domain;
  char          *id;
  char          *path;

  GFile         *cache;
  GFile         *config;
  GFile         *data;
};

G_DEFINE_FINAL_TYPE (ValentContext, valent_context, VALENT_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_DOMAIN,
  PROP_ID,
  PROP_PARENT,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };


static inline gboolean
ensure_directory (GFile *dir)
{
  g_assert (G_IS_FILE (dir));

  if (g_mkdir_with_parents (g_file_peek_path (dir), 0700) == -1)
    {
      VALENT_NOTE ("Failed to create \"%s\": %s",
                   g_file_peek_path (dir),
                   g_strerror (errno));
      return FALSE;
    }

  return TRUE;
}

static gboolean
remove_directory (GFile         *file,
                  GCancellable  *cancellable,
                  GError       **error)
{
  g_autoptr (GFileEnumerator) iter = NULL;

  g_assert (G_IS_FILE (file));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_assert (error == NULL || *error == NULL);

  iter = g_file_enumerate_children (file,
                                    G_FILE_ATTRIBUTE_STANDARD_NAME,
                                    G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                    cancellable,
                                    NULL);

  while (iter != NULL)
    {
      GFile *child;

      if (!g_file_enumerator_iterate (iter, NULL, &child, cancellable, error))
        return FALSE;

      if (child == NULL)
        break;

      if (!remove_directory (child, cancellable, error))
        return FALSE;
    }

  return g_file_delete (file, cancellable, error);
}

/*
 * GObject
 */
static void
valent_context_constructed (GObject *object)
{
  ValentContext *self = VALENT_CONTEXT (object);

  G_OBJECT_CLASS (valent_context_parent_class)->constructed (object);

  if (self->parent != NULL)
    {
      const char *base_path;

      base_path = valent_context_get_path (self->parent);
      self->path = g_build_filename (base_path, self->domain, self->id, NULL);
    }
  else
    {
      self->path = g_build_filename (self->domain, self->id, NULL);
    }

  self->cache = g_file_new_build_filename (g_get_user_cache_dir (),
                                           PACKAGE_NAME,
                                           self->path,
                                           NULL);
  self->config = g_file_new_build_filename (g_get_user_config_dir (),
                                            PACKAGE_NAME,
                                            self->path,
                                            NULL);
  self->data = g_file_new_build_filename (g_get_user_data_dir (),
                                          PACKAGE_NAME,
                                          self->path,
                                          NULL);
}

static void
valent_context_destroy (ValentObject *object)
{
  ValentContext *self = VALENT_CONTEXT (object);

  g_clear_object (&self->cache);
  g_clear_object (&self->config);
  g_clear_object (&self->data);
  g_clear_pointer (&self->path, g_free);
  g_clear_pointer (&self->domain, g_free);
  g_clear_pointer (&self->id, g_free);

  VALENT_OBJECT_CLASS (valent_context_parent_class)->destroy (object);
}

static void
valent_context_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  ValentContext *self = VALENT_CONTEXT (object);

  switch (prop_id)
    {
    case PROP_DOMAIN:
      g_value_set_string (value, valent_context_get_domain (self));
      break;

    case PROP_ID:
      g_value_set_string (value, valent_context_get_id (self));
      break;

    case PROP_PARENT:
      g_value_set_object (value, valent_context_get_parent (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_context_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  ValentContext *self = VALENT_CONTEXT (object);

  switch (prop_id)
    {
    case PROP_DOMAIN:
      self->domain = g_value_dup_string (value);
      break;

    case PROP_ID:
      self->id = g_value_dup_string (value);
      break;

    case PROP_PARENT:
      self->parent = g_value_get_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_context_class_init (ValentContextClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentObjectClass *vobject_class = VALENT_OBJECT_CLASS (klass);

  object_class->constructed = valent_context_constructed;
  object_class->get_property = valent_context_get_property;
  object_class->set_property = valent_context_set_property;

  vobject_class->destroy = valent_context_destroy;

  /**
   * ValentContext:domain: (getter get_domain)
   *
   * The domain for this context.
   *
   * Since: 1.0
   */
  properties [PROP_DOMAIN] =
    g_param_spec_string ("domain", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentContext:id: (getter get_id)
   *
   * The ID for this context.
   *
   * Since: 1.0
   */
  properties [PROP_ID] =
    g_param_spec_string ("id", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentContext:parent: (getter get_parent)
   *
   * The parent context.
   *
   * Since: 1.0
   */
  properties [PROP_PARENT] =
    g_param_spec_object ("parent", NULL, NULL,
                         VALENT_TYPE_CONTEXT,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
valent_context_init (ValentContext *self)
{
}

/**
 * valent_context_new:
 * @parent: (nullable): a parent context
 * @domain: (nullable): a domain
 * @id: (nullable): a unique identifier
 *
 * Create a new `ValentContext`.
 *
 * If given, @parent will be taken into consideration when building paths.
 *
 * If given, @domain should be an identifier describing the scope of the
 * contexts that will share it.
 *
 * If given, @id should be an identifier that is at least unique to @domain,
 * even if @domain is %NULL.
 *
 * Returns: (transfer full): a new `ValentContext`.
 *
 * Since: 1.0
 */
ValentContext *
valent_context_new (ValentContext *parent,
                    const char    *domain,
                    const char    *id)
{
  return g_object_new (VALENT_TYPE_CONTEXT,
                       "domain", domain,
                       "id",     id,
                       "parent", parent,
                       NULL);
}

/**
 * valent_context_get_domain: (get-property domain)
 * @context: a `ValentContext`
 *
 * Get the context domain.
 *
 * Returns: (transfer none): the context domain
 *
 * Since: 1.0
 */
const char *
valent_context_get_domain (ValentContext *context)
{
  g_return_val_if_fail (VALENT_IS_CONTEXT (context), NULL);

  return context->domain;
}

/**
 * valent_context_get_id: (get-property id)
 * @context: a `ValentContext`
 *
 * Get the context ID.
 *
 * Returns: (transfer none): the context ID
 *
 * Since: 1.0
 */
const char *
valent_context_get_id (ValentContext *context)
{
  g_return_val_if_fail (VALENT_IS_CONTEXT (context), NULL);

  return context->id;
}

/**
 * valent_context_get_parent: (get-property parent)
 * @context: a `ValentContext`
 *
 * Get the parent context.
 *
 * Returns: (transfer none) (nullable): a `ValentContext`
 */
ValentContext *
valent_context_get_parent (ValentContext *context)
{
  g_return_val_if_fail (VALENT_IS_CONTEXT (context), NULL);

  return context->parent;
}

/**
 * valent_context_get_path:
 * @context: a `ValentContext`
 *
 * Get the virtual path.
 *
 * Returns: (transfer none): a relative path
 *
 * Since: 1.0
 */
const char *
valent_context_get_path (ValentContext *context)
{
  g_return_val_if_fail (VALENT_IS_CONTEXT (context), NULL);

  return context->path;
}

/**
 * valent_context_clear_cache:
 * @context: a `ValentContext`
 *
 * Clear cache data.
 *
 * The method will remove all files in the cache directory.
 *
 * Since: 1.0
 */
void
valent_context_clear_cache (ValentContext *context)
{
  g_autoptr (GError) error = NULL;

  g_return_if_fail (VALENT_IS_CONTEXT (context));

  if (!remove_directory (context->cache, NULL, &error) &&
      !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
    g_warning ("%s(): %s", G_STRFUNC, error->message);
}

/**
 * valent_context_clear:
 * @context: a `ValentContext`
 *
 * Clear cache and configuration data.
 *
 * The method will remove all files in the cache and configuration directories.
 *
 * Since: 1.0
 */
void
valent_context_clear (ValentContext *context)
{
  g_autoptr (GError) error = NULL;

  g_return_if_fail (VALENT_IS_CONTEXT (context));

  /* FIXME: We have to be careful not to remove device config directories */
  if (context->domain == NULL)
    return;

  if (!remove_directory (context->cache, NULL, &error) &&
      !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
    g_warning ("%s(): %s", G_STRFUNC, error->message);
  g_clear_error (&error);

  if (!remove_directory (context->config, NULL, &error) &&
      !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
    g_warning ("%s(): %s", G_STRFUNC, error->message);
  g_clear_error (&error);
}

/**
 * valent_context_get_cache_file:
 * @context: a `ValentContext`
 * @filename: (type filename): a filename
 *
 * Create a new cache file.
 *
 * This method creates a new [iface@Gio.File] for @filename in the cache
 * directory.
 *
 * Returns: (transfer full) (nullable): a new `GFile`
 *
 * Since: 1.0
 */
GFile *
valent_context_get_cache_file (ValentContext *context,
                               const char    *filename)
{
  g_return_val_if_fail (VALENT_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (ensure_directory (context->cache), NULL);
  g_return_val_if_fail (filename != NULL && *filename != '\0', NULL);

  return g_file_get_child (context->cache, filename);
}

/**
 * valent_context_get_config_file:
 * @context: a `ValentContext`
 * @filename: (type filename): a filename
 *
 * Create a new config file.
 *
 * This method creates a new [iface@Gio.File] for @filename in the config
 * directory.
 *
 * Returns: (transfer full) (nullable): a new `GFile`
 *
 * Since: 1.0
 */
GFile *
valent_context_get_config_file (ValentContext *context,
                                const char    *filename)
{
  g_return_val_if_fail (VALENT_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (ensure_directory (context->config), NULL);
  g_return_val_if_fail (filename != NULL && *filename != '\0', NULL);

  return g_file_get_child (context->config, filename);
}

/**
 * valent_context_get_data_file:
 * @context: a `ValentContext`
 * @filename: (type filename): a filename
 *
 * Create a new data file.
 *
 * This method creates a new [iface@Gio.File] for @filename in the data
 * directory.
 *
 * Returns: (transfer full) (nullable): a new `GFile`
 *
 * Since: 1.0
 */
GFile *
valent_context_get_data_file (ValentContext *context,
                              const char    *filename)
{
  g_return_val_if_fail (VALENT_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (ensure_directory (context->data), NULL);
  g_return_val_if_fail (filename != NULL && *filename != '\0', NULL);

  return g_file_get_child (context->data, filename);
}

/**
 * valent_context_get_plugin_context:
 * @context: a `ValentContext`
 * @plugin_info: a `PeasPluginInfo`
 *
 * Create a new `ValentContext` for a plugin.
 *
 * If given, @domain should be an identifier describing the scope of the
 * contexts that will share it.
 *
 * If given, @id should be an identifier that is at least unique to @domain,
 * even if @domain is %NULL.
 *
 * Returns: (transfer full): a new `ValentContext`.
 *
 * Since: 1.0
 */
ValentContext *
valent_context_get_plugin_context (ValentContext  *context,
                                   PeasPluginInfo *plugin_info)
{
  g_return_val_if_fail (VALENT_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (plugin_info != NULL, NULL);

  return g_object_new (VALENT_TYPE_CONTEXT,
                       "parent", context,
                       "domain", "plugin",
                       "id",     peas_plugin_info_get_module_name (plugin_info),
                       NULL);
}

/**
 * valent_context_get_plugin_settings:
 * @context: a `ValentContext`
 * @plugin_info: a `PeasPluginInfo`
 * @plugin_key: an external data key
 *
 * Create a [class@Gio.Settings] object for a plugin.
 *
 * If the plugin is not built-in, an attempt will be made to compile a
 * [struct@Gio.SettingsSchema] for @schema_id, in the module directory of
 * @plugin_info.
 *
 * Returns: (transfer full) (nullable): the new `GSettings` object
 *
 * Since: 1.0
 */
GSettings *
valent_context_get_plugin_settings (ValentContext  *context,
                                    PeasPluginInfo *plugin_info,
                                    const char     *plugin_key)
{
  GSettingsSchemaSource *default_source;
  const char *schema_id = NULL;
  g_autoptr (GSettingsSchema) schema = NULL;
  g_autofree char *path = NULL;

  g_return_val_if_fail (VALENT_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (plugin_info != NULL, NULL);
  g_return_val_if_fail (plugin_key != NULL && *plugin_key != '\0', NULL);

  schema_id = peas_plugin_info_get_external_data (plugin_info, plugin_key);

  if (schema_id == NULL || *schema_id == '\0')
    return NULL;

  /* Check the default schema source first */
  default_source = g_settings_schema_source_get_default ();
  schema = g_settings_schema_source_lookup (default_source, schema_id, TRUE);

  /* Adapted from `peas-plugin-info.c` (LGPL-2.1-or-later) */
  if (schema == NULL)
    {
      g_autoptr (GSettingsSchemaSource) source = NULL;
      g_autoptr (GFile) gschema_compiled = NULL;
      const char *module_dir = NULL;

      module_dir = peas_plugin_info_get_module_dir (plugin_info);
      gschema_compiled = g_file_new_build_filename (module_dir,
                                                    "gschemas.compiled",
                                                    NULL);

      if (!g_file_query_exists (gschema_compiled, NULL))
        {
          const char *argv[] = {
            "glib-compile-schemas",
            "--targetdir", module_dir,
            module_dir,
            NULL
          };

          g_spawn_sync (NULL, (char **)argv, NULL, G_SPAWN_SEARCH_PATH,
                        NULL, NULL, NULL, NULL, NULL, NULL);
        }

      source = g_settings_schema_source_new_from_directory (module_dir,
                                                            default_source,
                                                            FALSE,
                                                            NULL);

      if (source != NULL)
        schema = g_settings_schema_source_lookup (source, schema_id, TRUE);
    }

  if (schema == NULL)
    {
      g_critical ("Settings schema '%s' not installed", schema_id);
      return NULL;
    }

  path = g_strdup_printf ("/ca/andyholmes/valent/%s/", context->path);

  return g_settings_new_full (schema, NULL, path);
}

/**
 * valent_context_create_settings:
 * @context: a `ValentContext`
 * @schema_id: a `GSettings` schema ID
 *
 * Create a [class@Gio.Settings] object.
 *
 * This is a simple wrapper around [ctor@Gio.Settings.new_full] that creates a
 * `GSettings` object for the path of @context. No attempt will be made to
 * find or compile missing schemas.
 *
 * Returns: (transfer full) (nullable): the new `GSettings` object
 *
 * Since: 1.0
 */
GSettings *
valent_context_create_settings (ValentContext *context,
                                const char    *schema_id)
{
  GSettingsSchemaSource *default_source;
  g_autoptr (GSettingsSchema) schema = NULL;
  g_autofree char *path = NULL;

  g_return_val_if_fail (VALENT_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (schema_id != NULL && *schema_id != '\0', NULL);

  default_source = g_settings_schema_source_get_default ();
  schema = g_settings_schema_source_lookup (default_source, schema_id, TRUE);

  if (schema == NULL)
    {
      g_critical ("Settings schema '%s' not installed", schema_id);
      return NULL;
    }

  path = g_strdup_printf ("/ca/andyholmes/valent/%s/", context->path);

  return g_settings_new_full (schema, NULL, path);
}

