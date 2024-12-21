// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-data-source"

#include "config.h"

#include <gio/gio.h>
#include <libpeas.h>

#define G_SETTINGS_ENABLE_BACKEND
#include <gio/gsettingsbackend.h>

#include "valent-debug.h"
#include "valent-macros.h"
#include "valent-object.h"
#include "valent-resource.h"

#include "valent-data-source.h"

#define VALENT_SETTINGS_BASE_PATH     "/ca/andyholmes/valent/"
#define VALENT_SETTINGS_PLUGIN_SCHEMA "ca.andyholmes.Valent.Plugin"

/**
 * ValentDataSource:
 *
 * A class representing an abstract data source.
 *
 * `ValentDataSource` is a run-time representation of a data context, inspired
 * by the DataSource class in the NEPOMUK Information Element Ontology. It
 * provides persistent data storage of various types by mapping onto existing
 * hierarchies like [class@Gio.Settings] (i.e. relocatable schemas), SPARQL
 * graphs and on-disk caches (i.e. XDG user dirs).
 *
 * Since: 1.0
 */

typedef struct
{
  char             *path;
  char             *source_mode;

  /* config */
  GFile            *config;
  GSettingsBackend *settings_backend;

  /* cache */
  GFile            *cache;
} ValentDataSourcePrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (ValentDataSource, valent_data_source, VALENT_TYPE_RESOURCE)

typedef enum
{
  PROP_SOURCE_MODE = 1,
} ValentDataSourceProperty;

static GParamSpec *properties[PROP_SOURCE_MODE + 1] = { NULL, };

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

static inline ValentDataSource *
valent_data_source_get_parent (ValentDataSource *self)
{
  ValentDataSource *parent = NULL;

  parent = valent_resource_get_source (VALENT_RESOURCE (self));
  while (parent != NULL && !VALENT_IS_DATA_SOURCE (parent))
    parent = valent_resource_get_source (VALENT_RESOURCE (parent));

  return parent;
}

static char *
valent_data_source_build_path  (ValentDataSource *self)
{
  ValentDataSource *parent = NULL;
  const char *identifier = NULL;

  identifier = valent_resource_get_identifier (VALENT_RESOURCE (self));
  if (identifier != NULL && *identifier == '\0')
    identifier = NULL;

  parent = valent_data_source_get_parent (self);
  if (VALENT_IS_DATA_SOURCE (parent))
    {
      ValentDataSourcePrivate *parent_priv = NULL;

      parent_priv = valent_data_source_get_instance_private (self);
      if (parent_priv->path != NULL)
        return g_build_filename (parent_priv->path, identifier, NULL);
    }

  if (identifier != NULL && *identifier != '\0')
    {
      return g_build_filename (identifier, NULL);
    }

  return NULL;
}

/*
 * ValentDataSource
 */
static GSettingsBackend *
valent_data_source_get_settings_backend (ValentDataSource *self)
{
  ValentDataSourcePrivate *priv = valent_data_source_get_instance_private (self);

  g_assert (VALENT_IS_DATA_SOURCE (self));

  if (priv->settings_backend == NULL)
    {
      g_autofree char *filename = NULL;

      filename = g_build_filename (g_file_peek_path (priv->config),
                                   "settings",
                                   "keyfile",
                                   NULL);
      priv->settings_backend = g_keyfile_settings_backend_new (filename,
                                                               "/ca/andyholmes/valent/",
                                                               NULL /* root group */);
      g_return_val_if_fail (G_IS_SETTINGS_BACKEND (priv->settings_backend), NULL);
    }

  return priv->settings_backend;
}

/*
 * GObject
 */
static void
valent_data_source_constructed (GObject *object)
{
  ValentDataSource *self = VALENT_DATA_SOURCE (object);
  ValentDataSourcePrivate *priv = valent_data_source_get_instance_private (self);

  G_OBJECT_CLASS (valent_data_source_parent_class)->constructed (object);

  priv->path = valent_data_source_build_path (self);
  priv->cache = g_file_new_build_filename (g_get_user_cache_dir (),
                                           PACKAGE_NAME,
                                           priv->path,
                                           NULL);
  priv->config = g_file_new_build_filename (g_get_user_config_dir (),
                                            PACKAGE_NAME,
                                            priv->path,
                                            NULL);
}

static void
valent_data_source_finalize (GObject *object)
{
  ValentDataSource *self = VALENT_DATA_SOURCE (object);
  ValentDataSourcePrivate *priv = valent_data_source_get_instance_private (self);

  g_clear_object (&priv->cache);
  g_clear_object (&priv->config);
  g_clear_object (&priv->settings_backend);
  g_clear_pointer (&priv->path, g_free);
  g_clear_pointer (&priv->source_mode, g_free);

  G_OBJECT_CLASS (valent_data_source_parent_class)->finalize (object);
}

static void
valent_data_source_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  ValentDataSource *self = VALENT_DATA_SOURCE (object);
  ValentDataSourcePrivate *priv = valent_data_source_get_instance_private (self);

  switch ((ValentDataSourceProperty)prop_id)
    {
    case PROP_SOURCE_MODE:
      g_value_set_string (value, priv->source_mode);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_data_source_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  ValentDataSource *self = VALENT_DATA_SOURCE (object);

  switch (prop_id)
    {
    case PROP_SOURCE_MODE:
      valent_data_source_set_source_mode (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_data_source_class_init (ValentDataSourceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = valent_data_source_constructed;
  object_class->finalize = valent_data_source_finalize;
  object_class->get_property = valent_data_source_get_property;
  object_class->set_property = valent_data_source_set_property;

  /**
   * ValentDataSource:source-mode: (getter get_source_mode) (setter set_source_mode)
   *
   * Represents a number of applicable modes for a data source.
   *
   * Representation for a standard set of device/application/service modes,
   * corresponding to various sets of modes that are either inbuilt in a device
   * (e.g. inbuilt phone modes such as silent, loud, general, vibrate, etc.) or
   * available for applications and online services (e.g. IM system modes such
   * as busy, available, invisible, etc.)
   *
   * Since: 1.0
   */
  properties[PROP_SOURCE_MODE] =
    g_param_spec_string ("source-mode", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, G_N_ELEMENTS (properties), properties);
}

static void
valent_data_source_init (ValentDataSource *self)
{
}

/**
 * valent_data_source_get_source_mode: (get-property source-mode)
 * @source: a `ValentDataSource`
 *
 * Get the source mode for @source.
 *
 * Returns: (transfer none) (nullable): a source mode
 *
 * Since: 1.0
 */
const char *
valent_data_source_get_source_mode (ValentDataSource *source)
{
  ValentDataSourcePrivate *priv = valent_data_source_get_instance_private (source);

  g_return_val_if_fail (VALENT_IS_DATA_SOURCE (source), NULL);

  return priv->source_mode;
}

/**
 * valent_data_source_set_source_mode: (set-property source-mode)
 * @source: a `ValentDataSource`
 * @mode: (nullable): a source mode
 *
 * Set the source mode to @mode.
 *
 * Since: 1.0
 */
void
valent_data_source_set_source_mode (ValentDataSource *source,
                                    const char       *mode)
{
  ValentDataSourcePrivate *priv = valent_data_source_get_instance_private (source);

  g_return_if_fail (VALENT_IS_DATA_SOURCE (source));

  if (g_set_str (&priv->source_mode, mode))
    g_object_notify_by_pspec (G_OBJECT (source), properties[PROP_SOURCE_MODE]);
}

/**
 * valent_data_source_clear_cache:
 * @source: a `ValentDataSource`
 *
 * Clear cache data.
 *
 * The method will remove all files in the cache directory.
 *
 * Since: 1.0
 */
void
valent_data_source_clear_cache (ValentDataSource *source)
{
  ValentDataSourcePrivate *priv = valent_data_source_get_instance_private (source);
  g_autoptr (GError) error = NULL;

  g_return_if_fail (VALENT_IS_DATA_SOURCE (source));

  if (!remove_directory (priv->cache, NULL, &error) &&
      !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
    g_warning ("%s(): %s", G_STRFUNC, error->message);
}

/**
 * valent_data_source_clear_data:
 * @source: a `ValentDataSource`
 *
 * Clear cache and configuration data.
 *
 * The method will remove all files in the cache and configuration directories.
 *
 * Since: 1.0
 */
void
valent_data_source_clear_data (ValentDataSource *source)
{
  ValentDataSourcePrivate *priv = valent_data_source_get_instance_private (source);
  g_autoptr (GError) error = NULL;

  g_return_if_fail (VALENT_IS_DATA_SOURCE (source));

  if (!remove_directory (priv->cache, NULL, &error) &&
      !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
    g_warning ("%s(): %s", G_STRFUNC, error->message);
  g_clear_error (&error);

  if (!remove_directory (priv->config, NULL, &error) &&
      !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
    g_warning ("%s(): %s", G_STRFUNC, error->message);
  g_clear_error (&error);
}

/**
 * valent_data_source_get_cache_file:
 * @source: a `ValentDataSource`
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
valent_data_source_get_cache_file (ValentDataSource *source,
                                   const char       *filename)
{
  ValentDataSourcePrivate *priv = valent_data_source_get_instance_private (source);

  g_return_val_if_fail (VALENT_IS_DATA_SOURCE (source), NULL);
  g_return_val_if_fail (ensure_directory (priv->cache), NULL);
  g_return_val_if_fail (filename != NULL && *filename != '\0', NULL);

  return g_file_get_child (priv->cache, filename);
}

/**
 * valent_data_source_get_config_file:
 * @source: a `ValentDataSource`
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
valent_data_source_get_config_file (ValentDataSource *source,
                                    const char       *filename)
{
  ValentDataSourcePrivate *priv = valent_data_source_get_instance_private (source);

  g_return_val_if_fail (VALENT_IS_DATA_SOURCE (source), NULL);
  g_return_val_if_fail (ensure_directory (priv->config), NULL);
  g_return_val_if_fail (filename != NULL && *filename != '\0', NULL);

  return g_file_get_child (priv->config, filename);
}

/**
 * valent_data_source_get_settings_full:
 * @source: a `ValentDataSource`
 * @schema: a `GSettingsSchema`
 * @path: a GSettings path
 *
 * Create a [class@Gio.Settings] object for @source.
 *
 * This is a wrapper around [ctor@Gio.Settings.new_full] that creates a
 * `GSettings` object with the [class@Gio.SettingsBackend] for @source.
 *
 * Returns: (transfer full) (nullable): a new `GFile`
 *
 * Since: 1.0
 */
GSettings *
valent_data_source_get_settings_full (ValentDataSource *source,
                                      GSettingsSchema  *schema,
                                      const char       *path)
{
  GSettingsBackend *backend;

  g_return_val_if_fail (VALENT_IS_DATA_SOURCE (source), NULL);
  g_return_val_if_fail (schema != NULL, NULL);
  g_return_val_if_fail (path != NULL && *path != '\0', NULL);

  if (!g_str_has_prefix (path, VALENT_SETTINGS_BASE_PATH))
    {
      g_critical ("Settings path \"%s\" not a subpath of \"%s\"",
                  path, VALENT_SETTINGS_BASE_PATH);
      return NULL;
    }

  backend = valent_data_source_get_settings_backend (source);
  return g_settings_new_full (schema, backend, path);
}

/**
 * valent_data_source_get_plugin_settings:
 * @source: a `ValentDataSource`
 * @plugin_info: a `PeasPluginInfo`
 * @extension_schema: (nullable): an external data key
 * @extension_domain: the extension domain
 *
 * Create a [class@Gio.Settings] object for an extension.
 *
 * If @extension_schema is given, it should be a GSettings schema ID, or a key
 * in the `.plugin` file hold a GSettings schema ID. If %NULL, the
 * `ca.andyholmes.Valent.Plugin` schema will be used.
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
valent_data_source_get_plugin_settings (ValentDataSource *source,
                                        PeasPluginInfo   *plugin_info,
                                        const char       *extension_schema,
                                        const char       *extension_domain)
{
  GSettingsSchemaSource *default_source;
  const char *schema_id = NULL;
  g_autoptr (GSettingsSchema) schema = NULL;
  g_autofree char *path = NULL;

  g_return_val_if_fail (VALENT_IS_DATA_SOURCE (source), NULL);
  g_return_val_if_fail (PEAS_IS_PLUGIN_INFO (plugin_info), NULL);
  g_return_val_if_fail (extension_domain != NULL && *extension_domain != '\0', NULL);

  if (extension_schema != NULL)
    {
      schema_id = peas_plugin_info_get_external_data (plugin_info,
                                                      extension_schema);
      if (schema_id == NULL && g_application_id_is_valid (extension_schema))
        schema_id = extension_schema;
      else
        return NULL;
    }
  else
    {
      schema_id = VALENT_SETTINGS_PLUGIN_SCHEMA;
    }

  default_source = g_settings_schema_source_get_default ();
  schema = g_settings_schema_source_lookup (default_source, schema_id, TRUE);

  /* Adapted from `peas-plugin-info.c` (LGPL-2.1-or-later)
   */
  if (schema == NULL)
    {
      g_autoptr (GSettingsSchemaSource) schema_source = NULL;
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

      schema_source = g_settings_schema_source_new_from_directory (module_dir,
                                                                   default_source,
                                                                   FALSE,
                                                                   NULL);

      if (schema_source != NULL)
        schema = g_settings_schema_source_lookup (schema_source, schema_id, TRUE);

      if (schema == NULL)
        {
          g_critical ("Settings schema \"%s\" not installed", schema_id);
          return NULL;
        }
    }

  path = g_strdup_printf ("/ca/andyholmes/valent/plugin/%s/extension/%s/",
                          peas_plugin_info_get_module_name (plugin_info),
                          extension_domain);

  return valent_data_source_get_settings_full (source, schema, path);
}

/*
 * ValentLocalDevice
 */
#define VALENT_TYPE_LOCAL_DEVICE (valent_local_device_get_type())
G_DECLARE_FINAL_TYPE (ValentLocalDevice, valent_local_device, VALENT, LOCAL_DEVICE, ValentDataSource)

struct _ValentLocalDevice
{
  ValentDataSource  parent_instance;

  GSettings        *settings;
};

G_DEFINE_FINAL_TYPE (ValentLocalDevice, valent_local_device, VALENT_TYPE_DATA_SOURCE)

static void
valent_local_device_finalize (GObject *object)
{
  ValentLocalDevice *self = VALENT_LOCAL_DEVICE (object);

  g_clear_object (&self->settings);

  G_OBJECT_CLASS (valent_local_device_parent_class)->finalize (object);
}

static void
valent_local_device_class_init (ValentLocalDeviceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  /* object_class->constructed = valent_data_source_constructed; */
  object_class->finalize = valent_local_device_finalize;
}

static void
valent_local_device_init (ValentLocalDevice *self)
{
  g_autofree char *title = NULL;

  /* Connect the resource title to the main settings
   */
  self->settings = g_settings_new ("ca.andyholmes.Valent");
  g_settings_bind (self->settings, "name",
                   self,           "title",
                   G_SETTINGS_BIND_DEFAULT);

  /* Wake up the binding and default to the hostname
   */
  title = g_settings_get_string (self->settings, "name");
  if (title == NULL || *title == '\0')
    g_settings_set_string (self->settings, "name", g_get_host_name ());
}

static void
on_default_instance_destroy (ValentObject  *object,
                             ValentObject **default_instance)
{
  if (default_instance != NULL && *default_instance == object)
    g_clear_object (default_instance);
}

/**
 * valent_data_source_get_local_default:
 *
 * Get the root [class@Valent.DataSource] for the local host.
 *
 * Returns: (transfer none) (not nullable): the local root resource
 *
 * Since: 1.0
 */
ValentResource *
valent_data_source_get_local_default (void)
{
  static ValentResource *default_instance = NULL;

  if (default_instance == NULL)
    {
      GSettings *settings = NULL;
      g_autofree char *title = NULL;

      default_instance = g_object_new (VALENT_TYPE_LOCAL_DEVICE,
                                       "identifier", "localhost",
                                       NULL);
      g_signal_connect (default_instance,
                        "destroy",
                        G_CALLBACK (on_default_instance_destroy),
                        &default_instance);

      /* Connect the resource title to the main settings
       */
      settings = g_settings_new ("ca.andyholmes.Valent");
      g_settings_bind (settings,         "name",
                       default_instance, "title",
                       G_SETTINGS_BIND_DEFAULT);

      /* Wake up the binding and default to the hostname
       */
      title = g_settings_get_string (settings, "name");
      if (title == NULL || *title == '\0')
        g_settings_set_string (settings, "name", g_get_host_name ());
      else
        valent_resource_set_title (default_instance, title);

      g_object_set_data_full (G_OBJECT (default_instance),
                              "valent-settings",
                              g_steal_pointer (&settings),
                              g_object_unref);
    }

  return default_instance;
}

