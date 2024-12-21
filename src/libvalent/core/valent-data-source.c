// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-data-source"

#include "config.h"

#include <gio/gio.h>
#include <libtracker-sparql/tracker-sparql.h>

#define G_SETTINGS_ENABLE_BACKEND
#include <gio/gsettingsbackend.h>

#include "valent-debug.h"
#include "valent-macros.h"
#include "valent-object.h"
#include "valent-resource.h"

#include "valent-data-source.h"

#define VALENT_LOCAL_SOURCE_IDENTIFIER "localhost"
#define VALENT_SETTINGS_ROOT_PATH      "/ca/andyholmes/valent/"
#define VALENT_SETTINGS_ROOT_GROUP     "ca.andyholmes.valent"

/**
 * ValentDataSource:
 *
 * An abstract base class for data sources.
 *
 * `ValentDataSource` is a representation of a data source, inspired by the
 * DataSource class in the NEPOMUK Information Element Ontology. It provides
 * facilities for persistent storage, including a [type@Gio.SettingsBackend],
 * SPARQL database and on-disk caches (i.e. XDG user dirs).
 *
 * Since: 1.0
 */
typedef struct
{
  char                    *path;
  GFile                   *cache;
  GFile                   *config;
  GSettingsBackend        *settings_backend;
  TrackerSparqlConnection *sparql_connection;
} ValentDataSourcePrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (ValentDataSource, valent_data_source, VALENT_TYPE_RESOURCE)

typedef enum
{
  PROP_SOURCE_MODE = 1,
} ValentDataSourceProperty;

static GParamSpec *properties[PROP_SOURCE_MODE + 1] = { NULL, };

static gboolean
_g_file_delete_directory (GFile         *directory,
                          GCancellable  *cancellable,
                          GError       **error)
{
  g_autoptr (GFileEnumerator) iter = NULL;

  g_assert (G_IS_FILE (directory));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_assert (error == NULL || *error == NULL);

  iter = g_file_enumerate_children (directory,
                                    G_FILE_ATTRIBUTE_STANDARD_TYPE,
                                    G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                    cancellable,
                                    NULL);

  while (iter != NULL)
    {
      GFile *child;
      GFileInfo *info;

      if (!g_file_enumerator_iterate (iter, &info, &child, cancellable, error))
        return FALSE;

      if (child == NULL)
        break;

      if (g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY)
        {
          if (!_g_file_delete_directory (child, cancellable, error))
            return FALSE;
        }
      else if (!g_file_delete (child, cancellable, error))
        {
          return FALSE;
        }
    }

  return g_file_delete (directory, cancellable, error);
}

/*
 * ValentDataSource
 */
static const char *
valent_data_source_real_get_source_mode (ValentDataSource *source)
{
  g_assert (VALENT_IS_DATA_SOURCE (source));

  return NULL;
}

static void
valent_data_source_store_data (ValentDataSource *self)
{
  ValentDataSourcePrivate *priv = valent_data_source_get_instance_private (self);
  g_autoptr (GFile) cache_files = NULL;
  g_autofree char *settings_path = NULL;

  g_assert (VALENT_IS_DATA_SOURCE (self));

  /* Files returned by valent_data_source_get_cache_file() are located here
   */
  cache_files = g_file_get_child (priv->cache, "files");
  if (g_mkdir_with_parents (g_file_peek_path (cache_files), 0700) == -1)
    {
      g_critical ("%s(): failed to create \"%s\": %s",
                  G_STRFUNC,
                  g_file_peek_path (cache_files),
                  g_strerror (errno));
      return;
    }

  /* Creating the GSettingsBackend creates the directory path we need
   */
  settings_path = g_build_filename (g_file_peek_path (priv->config),
                                    "settings",
                                    "keyfile",
                                    NULL);
  priv->settings_backend = g_keyfile_settings_backend_new (settings_path,
                                                           VALENT_SETTINGS_ROOT_PATH,
                                                           VALENT_SETTINGS_ROOT_GROUP);
}

/*
 * GObject
 */
static void
valent_data_source_constructed (GObject *object)
{
  ValentDataSource *self = VALENT_DATA_SOURCE (object);
  ValentDataSourcePrivate *priv = valent_data_source_get_instance_private (self);
  const char *identifier = NULL;

  G_OBJECT_CLASS (valent_data_source_parent_class)->constructed (object);

  identifier = valent_resource_get_identifier (VALENT_RESOURCE (self));
  if (identifier != NULL && *identifier != '\0')
    {
      priv->cache = g_file_new_build_filename (g_get_user_cache_dir (),
                                               PACKAGE_NAME,
                                               "sources",
                                               identifier,
                                               NULL);
      priv->config = g_file_new_build_filename (g_get_user_config_dir (),
                                                PACKAGE_NAME,
                                                "sources",
                                                identifier,
                                                NULL);
    }
  else
    {
      priv->cache = g_file_new_build_filename (g_get_user_cache_dir (),
                                               PACKAGE_NAME,
                                               NULL);
      priv->config = g_file_new_build_filename (g_get_user_config_dir (),
                                                PACKAGE_NAME,
                                                NULL);
    }

  valent_data_source_store_data (self);
}

static void
valent_data_source_finalize (GObject *object)
{
  ValentDataSource *self = VALENT_DATA_SOURCE (object);
  ValentDataSourcePrivate *priv = valent_data_source_get_instance_private (self);

  g_clear_object (&priv->cache);
  g_clear_object (&priv->config);
  g_clear_object (&priv->settings_backend);
  g_clear_object (&priv->sparql_connection);

  G_OBJECT_CLASS (valent_data_source_parent_class)->finalize (object);
}

static void
valent_data_source_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  ValentDataSource *self = VALENT_DATA_SOURCE (object);

  switch ((ValentDataSourceProperty)prop_id)
    {
    case PROP_SOURCE_MODE:
      g_value_set_string (value, valent_data_source_get_source_mode (self));
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

  klass->get_source_mode = valent_data_source_real_get_source_mode;

  /**
   * ValentDataSource:source-mode: (getter get_source_mode)
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
                         (G_PARAM_READABLE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, G_N_ELEMENTS (properties), properties);
}

static void
valent_data_source_init (ValentDataSource *self)
{
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

  if (!_g_file_delete_directory (priv->cache, NULL, &error) &&
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

  if (!_g_file_delete_directory (priv->cache, NULL, &error) &&
      !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
    g_warning ("%s(): %s", G_STRFUNC, error->message);
  g_clear_error (&error);

  if (!_g_file_delete_directory (priv->config, NULL, &error) &&
      !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
    g_warning ("%s(): %s", G_STRFUNC, error->message);
  g_clear_error (&error);
}

/**
 * valent_data_source_get_cache_directory:
 * @source: a `ValentDataSource`
 *
 * Get the cache directory for @source.
 *
 * This method returns the base cache directory, containing subfolders for
 * cached files and the SPARQL database. To get a new file in the `files/`
 * directory, call [method@Valent.DataSource.get_cache_file].
 *
 * Returns: (transfer none) (not nullable): a new `GFile`
 *
 * Since: 1.0
 */
GFile *
valent_data_source_get_cache_directory (ValentDataSource *source)
{
  ValentDataSourcePrivate *priv = valent_data_source_get_instance_private (source);

  g_return_val_if_fail (VALENT_IS_DATA_SOURCE (source), NULL);

  return priv->cache;
}

/**
 * valent_data_source_get_cache_file:
 * @source: a `ValentDataSource`
 * @filename: (type filename): a filename
 *
 * Create a new cache file.
 *
 * This method creates a new [iface@Gio.File] for @filename in the `files/`
 * subfolder of the cache directory. Callers may choose to add these files
 * to the unnamed graph for @source.
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
  g_return_val_if_fail (filename != NULL && *filename != '\0', NULL);

  return g_file_new_build_filename (g_file_peek_path (priv->cache),
                                    "files",
                                    filename,
                                    NULL);
}

/**
 * valent_data_source_get_config_directory:
 * @source: a `ValentDataSource`
 *
 * Get the config directory for @source.
 *
 * This method returns the base config directory, containing a subfolder for
 * the [type@Gio.SettingsSchemaBackend]. To get a new file in the config
 * directory, call [method@Valent.DataSource.get_config_file].
 *
 * Returns: (transfer none) (not nullable): a new `GFile`
 *
 * Since: 1.0
 */
GFile *
valent_data_source_get_config_directory (ValentDataSource *source)
{
  ValentDataSourcePrivate *priv = valent_data_source_get_instance_private (source);

  g_return_val_if_fail (VALENT_IS_DATA_SOURCE (source), NULL);

  return priv->config;
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
  g_return_val_if_fail (filename != NULL && *filename != '\0', NULL);

  return g_file_get_child (priv->config, filename);
}

/**
 * valent_data_source_get_settings_backend:
 * @source: a `ValentDataSource`
 *
 * Get the [class@Gio.SettingsBackend] for @source.
 *
 * This can be passed to [ctor@Gio.Settings.new_full] to store settings local
 * to the @source.
 *
 * Returns: (transfer none): the settings backend
 *
 * Since: 1.0
 */
GSettingsBackend *
valent_data_source_get_settings_backend (ValentDataSource *source)
{
  ValentDataSourcePrivate *priv = valent_data_source_get_instance_private (source);

  g_return_val_if_fail (VALENT_IS_DATA_SOURCE (source), NULL);

  return priv->settings_backend;
}

/**
 * valent_data_source_get_source_mode: (get-property source-mode) (virtual get_source_mode)
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
  g_return_val_if_fail (VALENT_IS_DATA_SOURCE (source), NULL);

  return VALENT_DATA_SOURCE_GET_CLASS (source)->get_source_mode (source);
}

static void
tracker_sparql_connection_new_async_cb (GObject      *object,
                                        GAsyncResult *result,
                                        gpointer      user_data)
{
  g_autoptr (GTask) task = G_TASK (g_steal_pointer (&user_data));
  ValentDataSource *self = g_task_get_source_object (task);
  ValentDataSourcePrivate *priv = valent_data_source_get_instance_private (self);
  TrackerSparqlConnection *connection;
  GError *error = NULL;

  connection = tracker_sparql_connection_new_finish (result, &error);
  if (connection == NULL)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  g_set_object (&priv->sparql_connection, connection);
  g_task_return_pointer (task, g_steal_pointer (&connection), g_object_unref);
}

/**
 * valent_data_source_get_sparql_connection:
 * @source: a `ValentDataSource`
 * @cancellable: (nullable): a `GCancellable`
 * @callback: (scope async): a `GAsyncReadyCallback`
 * @user_data: user supplied data
 *
 * Open a connection to the SPARQL graph for @source.
 *
 * Call [method@Valent.DataSource.get_sparql_connection_finish] to get the result.
 *
 * Since: 1.0
 */
void
valent_data_source_get_sparql_connection (ValentDataSource    *source,
                                          GCancellable        *cancellable,
                                          GAsyncReadyCallback  callback,
                                          gpointer             user_data)
{
  ValentDataSourcePrivate *priv = valent_data_source_get_instance_private (source);
  g_autoptr (GTask) task = NULL;

  g_return_if_fail (VALENT_IS_DATA_SOURCE (source));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (source, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_data_source_get_sparql_connection);

  if (priv->sparql_connection == NULL)
    {
      g_autoptr (GFile) store = NULL;
      g_autoptr (GFile) ontology = NULL;

      store = g_file_get_child (priv->cache, "metadata");
      ontology = g_file_new_for_uri ("resource:///ca/andyholmes/Valent/ontologies/");
      tracker_sparql_connection_new_async (TRACKER_SPARQL_CONNECTION_FLAGS_NONE,
                                           store,
                                           ontology,
                                           cancellable,
                                           tracker_sparql_connection_new_async_cb,
                                           g_object_ref (task));
      return;
    }

  g_task_return_pointer (task,
                         g_object_ref (priv->sparql_connection),
                         g_object_unref);
}

/**
 * valent_data_source_get_sparql_connection_finish:
 * @source: a `ValentDataSource`
 * @result: a `GAsyncResult`
 * @error: (nullable): a `GError`
 *
 * Open a SPARQL connection for @source.
 *
 * Finish an operation started by [method@Valent.DataSource.get_sparql_connection].
 *
 * Returns: (transfer full) (nullable): a new `TrackerSparqlConnection`, or
 *   %NULL with @error set
 *
 * Since: 1.0
 */
TrackerSparqlConnection *
valent_data_source_get_sparql_connection_finish (ValentDataSource  *source,
                                                 GAsyncResult      *result,
                                                 GError           **error)
{
  g_return_val_if_fail (VALENT_IS_DATA_SOURCE (source), NULL);
  g_return_val_if_fail (g_task_is_valid (result, source), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}

/**
 * valent_data_source_get_sparql_connection_sync:
 * @source: a `ValentDataSource`
 * @cancellable: (nullable): a `GCancellable`
 * @error: (nullable): a `GError`
 *
 * Open a connection to the SPARQL graph for @source.
 *
 * Returns: (transfer full) (nullable): a new `TrackerSparqlConnection`, or
 *   %NULL with @error set
 *
 * Since: 1.0
 */
TrackerSparqlConnection *
valent_data_source_get_sparql_connection_sync (ValentDataSource  *source,
                                               GCancellable      *cancellable,
                                               GError           **error)
{
  ValentDataSourcePrivate *priv = valent_data_source_get_instance_private (source);

  g_return_val_if_fail (VALENT_IS_DATA_SOURCE (source), NULL);
  g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  if (priv->sparql_connection == NULL)
    {
      g_autoptr (GFile) store = NULL;
      g_autoptr (GFile) ontology = NULL;

      store = g_file_get_child (priv->cache, "metadata");
      ontology = g_file_new_for_uri ("resource:///ca/andyholmes/Valent/ontologies/");
      priv->sparql_connection =
        tracker_sparql_connection_new (TRACKER_SPARQL_CONNECTION_FLAGS_NONE,
                                       store,
                                       ontology,
                                       cancellable,
                                       error);

      if (priv->sparql_connection == NULL)
        return NULL;
    }

  return g_object_ref (priv->sparql_connection);
}

/*
 * ValentLocalSource
 */
#define VALENT_TYPE_LOCAL_SOURCE (valent_local_source_get_type())
G_DECLARE_FINAL_TYPE (ValentLocalSource, valent_local_source, VALENT, LOCAL_SOURCE, ValentDataSource)

struct _ValentLocalSource
{
  ValentDataSource  parent_instance;
};

G_DEFINE_FINAL_TYPE (ValentLocalSource, valent_local_source, VALENT_TYPE_DATA_SOURCE)

static void
valent_local_source_class_init (ValentLocalSourceClass *klass)
{
}

static void
valent_local_source_init (ValentLocalSource *self)
{
}

static void
on_data_source_destroyed (ValentObject  *object,
                          ValentObject **object_ptr)
{
  if (object_ptr != NULL && *object_ptr == object)
    g_clear_object (object_ptr);
}

/**
 * valent_data_source_get_default:
 *
 * Get the default [class@Valent.DataSource] for the current process.
 *
 * This data source represents the application generally, storing the global
 * [type@Gio.SettingsBackend] for schemas used with [class@Valent.Settings]
 * and other share resources.
 *
 * Returns: (transfer none) (not nullable): the local root resource
 *
 * Since: 1.0
 */
ValentDataSource *
valent_data_source_get_default (void)
{
  static ValentDataSource *default_instance = NULL;

  if (default_instance == NULL)
    {
      default_instance = g_object_new (VALENT_TYPE_LOCAL_SOURCE, NULL);
      g_signal_connect (default_instance,
                        "destroy",
                        G_CALLBACK (on_data_source_destroyed),
                        &default_instance);
    }

  return default_instance;
}

/**
 * valent_data_source_get_local_default:
 *
 * Get the [class@Valent.DataSource] for the localhost.
 *
 * This data source represents the local device, storing cached files and
 * configuration data.
 *
 * Returns: (transfer none) (not nullable): the local root resource
 *
 * Since: 1.0
 */
ValentDataSource *
valent_data_source_get_local_default (void)
{
  static ValentDataSource *local_instance = NULL;

  if (local_instance == NULL)
    {
      g_autoptr (GSettings) settings = NULL;
      g_autofree char *title = NULL;

      local_instance = g_object_new (VALENT_TYPE_LOCAL_SOURCE,
                                     "identifier", VALENT_LOCAL_SOURCE_IDENTIFIER,
                                     "source",     valent_data_source_get_default (),
                                     NULL);
      g_signal_connect (local_instance,
                        "destroy",
                        G_CALLBACK (on_data_source_destroyed),
                        &local_instance);

      /* Connect the resource title to the main settings, defaulting
       * to the hostname.
       */
      settings = g_settings_new ("ca.andyholmes.Valent");
      g_settings_bind (settings,       "name",
                       local_instance, "title",
                       G_SETTINGS_BIND_DEFAULT);

      // TODO: validate hostname as a device name
      title = g_settings_get_string (settings, "name");
      if (title == NULL || *title == '\0')
        g_settings_set_string (settings, "name", g_get_host_name ());

      g_object_set_data_full (G_OBJECT (local_instance),
                              "valent-settings",
                              g_steal_pointer (&settings),
                              g_object_unref);
    }

  return local_instance;
}

