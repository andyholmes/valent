// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-data"

#include "config.h"

#include <errno.h>
#include <gio/gio.h>
#include <sqlite3.h>

#include "valent-data.h"
#include "valent-debug.h"
#include "valent-macros.h"
#include "valent-utils.h"


/**
 * SECTION:valentdata
 * @short_description: An object for handling contextual data
 * @title: ValentData
 * @stability: Unstable
 * @include: libvalent-core.h
 *
 * The #ValentData class is intended to abstract storing and loading data for a
 * given context, such as a device or plugin.
 *
 * There are conveniences for working with #GFile, getting and settings file
 * contents and high-level `sqlite`.
 */

typedef struct
{
  ValentData *parent;

  char       *cache_path;
  char       *config_path;
  char       *data_path;
  char       *context;
} ValentDataPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (ValentData, valent_data, G_TYPE_OBJECT)

/**
 * ValentDataClass:
 * @sql_init: the virtual function pointer for valent_data_sql_init()
 *
 * The virtual function table for #ValentData.
 */


enum {
  PROP_0,
  PROP_CACHE_PATH,
  PROP_CONFIG_PATH,
  PROP_CONTEXT,
  PROP_DATA_PATH,
  PROP_PARENT,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

/* Ensure that sqlite3_int64 is the same size as gint64 */
G_STATIC_ASSERT (sizeof (sqlite3_int64) == sizeof (gint64));


static inline gboolean
ensure_directory (const char *path)
{
  g_assert (path != NULL && *path != '\0');

  if (g_mkdir_with_parents (path, 0700) == -1)
    {
      VALENT_NOTE ("Failed to create \"%s\": %s", path, g_strerror (errno));
      return FALSE;
    }

  return TRUE;
}

static gboolean
remove_directory (GFile         *file,
                  GCancellable  *cancellable,
                  GError       **error)
{
  g_autoptr (GFileEnumerator) fenum = NULL;

  g_assert (G_IS_FILE (file));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_assert (error == NULL || *error == NULL);

  fenum = g_file_enumerate_children (file,
                                     G_FILE_ATTRIBUTE_STANDARD_NAME,
                                     G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                     cancellable,
                                     NULL);

  while (fenum != NULL)
    {
      GFile *child;

      if (!g_file_enumerator_iterate (fenum, NULL, &child, cancellable, error))
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
valent_data_constructed (GObject *object)
{
  ValentData *self = VALENT_DATA (object);
  ValentDataPrivate *priv = valent_data_get_instance_private (self);

  /* A parent #ValentData object */
  if (priv->parent != NULL)
    {
      const char *base_path;

      base_path = valent_data_get_cache_path (priv->parent);
      priv->cache_path = g_build_filename (base_path, priv->context, NULL);

      base_path = valent_data_get_config_path (priv->parent);
      priv->config_path = g_build_filename (base_path, priv->context, NULL);

      base_path = valent_data_get_data_path (priv->parent);
      priv->data_path = g_build_filename (base_path, priv->context, NULL);
    }
  else
    {
      priv->cache_path = g_build_filename (g_get_user_cache_dir (),
                                           PACKAGE_NAME, priv->context,
                                           NULL);

      priv->config_path = g_build_filename (g_get_user_config_dir (),
                                            PACKAGE_NAME, priv->context,
                                            NULL);

      priv->data_path = g_build_filename (g_get_user_data_dir (),
                                          PACKAGE_NAME, priv->context,
                                          NULL);
    }

  G_OBJECT_CLASS (valent_data_parent_class)->constructed (object);
}

static void
valent_data_finalize (GObject *object)
{
  ValentData *self = VALENT_DATA (object);
  ValentDataPrivate *priv = valent_data_get_instance_private (self);

  g_clear_pointer (&priv->cache_path, g_free);
  g_clear_pointer (&priv->config_path, g_free);
  g_clear_pointer (&priv->data_path, g_free);
  g_clear_pointer (&priv->context, g_free);
  g_clear_object (&priv->parent);

  G_OBJECT_CLASS (valent_data_parent_class)->finalize (object);
}

static void
valent_data_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  ValentData *self = VALENT_DATA (object);

  switch (prop_id)
    {
    case PROP_CACHE_PATH:
      g_value_set_string (value, valent_data_get_cache_path (self));
      break;

    case PROP_CONFIG_PATH:
      g_value_set_string (value, valent_data_get_config_path (self));
      break;

    case PROP_CONTEXT:
      g_value_set_string (value, valent_data_get_context (self));
      break;

    case PROP_DATA_PATH:
      g_value_set_string (value, valent_data_get_data_path (self));
      break;

    case PROP_PARENT:
      g_value_set_object (value, valent_data_get_parent (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_data_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  ValentData *self = VALENT_DATA (object);
  ValentDataPrivate *priv = valent_data_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      priv->context = g_value_dup_string (value);
      break;

    case PROP_PARENT:
      priv->parent = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_data_class_init (ValentDataClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = valent_data_constructed;
  object_class->finalize = valent_data_finalize;
  object_class->get_property = valent_data_get_property;
  object_class->set_property = valent_data_set_property;

  /**
   * ValentData:cache-path:
   *
   * Path to the cache directory for this #ValentData.
   */
  properties [PROP_CACHE_PATH] =
    g_param_spec_string ("cache-path",
                         "Cache Path",
                         "Path to the cache directory",
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentData:config-path:
   *
   * Path to the config directory for this #ValentData.
   */
  properties [PROP_CONFIG_PATH] =
    g_param_spec_string ("config-path",
                         "Config Path",
                         "Path to the config directory",
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentData:context:
   *
   * The "context" property is the specific context for this #ValentData.
   *
   * For example, the application will generally have no context (%NULL),
   * therefore using the root of #ValentData:base-path , while devices or
   * backends will generally store their data in subdirectories of these.
   */
  properties [PROP_CONTEXT] =
    g_param_spec_string ("context",
                         "Context",
                         "Context to store data for",
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentData:data-path:
   *
   * Path to the data directory for this #ValentData.
   */
  properties [PROP_DATA_PATH] =
    g_param_spec_string ("data-path",
                         "Data Path",
                         "Path to the data directory",
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentData:parent:
   *
   * The "parent" #ValentData of this instance. If given during construction the
   * ancestor #ValentData will be taken into consideration when using path and
   * filenames.
   *
   * For example, a #ValentManager, with a #ValentData object, manages devices
   * with #ValentData objects that are children of the manager's data object,
   * and so on with plugins.
   */
  properties [PROP_PARENT] =
    g_param_spec_object ("parent",
                         "Parent",
                         "Parent data object",
                         VALENT_TYPE_DATA,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
valent_data_init (ValentData *self)
{
}

/**
 * valent_data_new:
 * @context: (nullable): a data context
 * @parent: (nullable): a #ValentData
 *
 * Create a new #ValentData for @context.
 *
 * Returns: (transfer full): a new #ValentData.
 */
ValentData *
valent_data_new (const char *context,
                 ValentData *parent)
{
  return g_object_new (VALENT_TYPE_DATA,
                       "context", context,
                       "parent",  parent,
                       NULL);
}

/**
 * valent_data_get_directory:
 * @directory: a #GUserDirectory
 *
 * Return a #GUserDirectory file path for @directory that is guaranteed to be
 * valid and existing.
 *
 * Returns: (transfer full): a directory path
 */
char *
valent_data_get_directory (GUserDirectory directory)
{
  g_autofree char *dirname = NULL;

  /* Ensure we get a real path */
  dirname = g_strdup (g_get_user_special_dir (directory));

  if (dirname == NULL)
    {
      switch (directory)
        {
        case G_USER_DIRECTORY_DESKTOP:
          dirname = g_build_filename (g_get_home_dir(), "Desktop", NULL);
          break;

        case G_USER_DIRECTORY_DOCUMENTS:
          dirname = g_build_filename (g_get_home_dir(), "Documents", NULL);
          break;

        case G_USER_DIRECTORY_DOWNLOAD:
          dirname = g_build_filename (g_get_home_dir(), "Downloads", NULL);
          break;

        case G_USER_DIRECTORY_MUSIC:
          dirname = g_build_filename (g_get_home_dir(), "Music", NULL);
          break;

        case G_USER_DIRECTORY_PICTURES:
          dirname = g_build_filename (g_get_home_dir(), "Pictures", NULL);
          break;

        case G_USER_DIRECTORY_PUBLIC_SHARE:
          dirname = g_build_filename (g_get_home_dir(), "Public", NULL);
          break;

        case G_USER_DIRECTORY_TEMPLATES:
          dirname = g_build_filename (g_get_home_dir(), "Templates", NULL);
          break;

        case G_USER_DIRECTORY_VIDEOS:
          dirname = g_build_filename (g_get_home_dir(), "Videos", NULL);
          break;

        default:
          dirname = g_strdup (g_get_home_dir());
          break;
        }
    }

  if (g_mkdir_with_parents (dirname, 0700) == -1)
    {
      int error = errno;

      g_critical ("%s(): creating \"%s\": %s",
                  G_STRFUNC,
                  dirname,
                  g_strerror (error));
    }

  return g_steal_pointer (&dirname);
}

/**
 * valent_data_get_file:
 * @dirname: a directory path
 * @basename: (type filename): a file name
 * @unique: whether the resulting filepath should be unique
 *
 * Create a new #GFile for @filename in @base_path. If @unique is %TRUE, the
 * file will be guaranteed not to conflict with an existing file.
 *
 * Returns: (transfer full): a #GFile
 */
GFile *
valent_data_get_file (const char *dirname,
                      const char *basename,
                      gboolean    unique)
{
  g_autofree char *basepath = NULL;
  g_autofree char *filepath = NULL;
  unsigned int copy_num = 0;

  g_return_val_if_fail (dirname != NULL, NULL);
  g_return_val_if_fail (basename != NULL, NULL);

  basepath = g_build_filename (dirname, basename, NULL);
  filepath = g_strdup (basepath);

  /* If a unique path is requested, loop until we find a free name */
  while (unique && g_file_test (filepath, G_FILE_TEST_EXISTS))
    {
      g_free (filepath);
      filepath = g_strdup_printf ("%s (%d)", basepath, ++copy_num);
    }

  return g_file_new_for_path (filepath);
}

/**
 * valent_data_new_cache_file:
 * @data: a #ValentData
 * @filename: (type filename): a filename
 *
 * Get a #GFile for @filename in the cache directory of @data.
 *
 * Returns: (transfer full) (nullable): a new #GFile
 */
GFile *
valent_data_new_cache_file (ValentData *data,
                            const char *filename)
{
  ValentDataPrivate *priv = valent_data_get_instance_private (data);

  g_return_val_if_fail (VALENT_IS_DATA (data), NULL);
  g_return_val_if_fail (ensure_directory (priv->cache_path), NULL);

  return g_file_new_build_filename (priv->cache_path, filename, NULL);
}

/**
 * valent_data_get_cache_path:
 * @data: a #ValentData
 *
 * Get the cache directory of @data as a string.
 *
 * Returns: (transfer none): a path
 */
const char *
valent_data_get_cache_path (ValentData *data)
{
  ValentDataPrivate *priv = valent_data_get_instance_private (data);

  g_return_val_if_fail (VALENT_IS_DATA (data), NULL);
  g_return_val_if_fail (ensure_directory (priv->cache_path), NULL);

  return priv->cache_path;
}

/**
 * valent_data_new_config_file:
 * @data: a #ValentData
 * @filename: (type filename): a filename
 *
 * Get a #GFile for @filename in the config directory of @data.
 *
 * Returns: (transfer full) (nullable): a new #GFile
 */
GFile *
valent_data_new_config_file (ValentData *data,
                             const char *filename)
{
  ValentDataPrivate *priv = valent_data_get_instance_private (data);

  g_return_val_if_fail (VALENT_IS_DATA (data), NULL);
  g_return_val_if_fail (ensure_directory (priv->config_path), NULL);

  return g_file_new_build_filename (priv->config_path, filename, NULL);
}

/**
 * valent_data_get_config_path:
 * @data: a #ValentData
 *
 * Get the config directory of @data as a string.
 *
 * Returns: (transfer none): a path
 */
const char *
valent_data_get_config_path (ValentData *data)
{
  ValentDataPrivate *priv = valent_data_get_instance_private (data);

  g_return_val_if_fail (VALENT_IS_DATA (data), NULL);
  g_return_val_if_fail (ensure_directory (priv->config_path), NULL);

  return priv->config_path;
}

/**
 * valent_data_get_context:
 * @data: a #ValentData
 *
 * Get the context of @data.
 *
 * Returns: (not nullable) (transfer none): the context
 */
const char *
valent_data_get_context (ValentData *data)
{
  ValentDataPrivate *priv = valent_data_get_instance_private (data);

  g_return_val_if_fail (VALENT_IS_DATA (data), NULL);

  return priv->context;
}

/**
 * valent_data_new_data_file:
 * @data: a #ValentData
 * @filename: (type filename): a filename
 *
 * Get a #GFile for @filename in the data directory of @data.
 *
 * Returns: (transfer full) (nullable): a new #GFile
 */
GFile *
valent_data_new_data_file (ValentData *data,
                           const char *filename)
{
  ValentDataPrivate *priv = valent_data_get_instance_private (data);

  g_return_val_if_fail (VALENT_IS_DATA (data), NULL);
  g_return_val_if_fail (ensure_directory (priv->data_path), NULL);

  return g_file_new_build_filename (priv->data_path, filename, NULL);
}

/**
 * valent_data_get_data_path:
 * @data: a #ValentData
 *
 * Get the data directory of @data as a string.
 *
 * Returns: (transfer none): a path
 */
const char *
valent_data_get_data_path (ValentData *data)
{
  ValentDataPrivate *priv = valent_data_get_instance_private (data);

  g_return_val_if_fail (VALENT_IS_DATA (data), NULL);
  g_return_val_if_fail (ensure_directory (priv->data_path), NULL);

  return priv->data_path;
}

/**
 * valent_data_get_parent:
 * @data: a #ValentData
 *
 * Get the parent #ValentData of @data. If %NULL is returned, then @data is
 * equivalent to or a reference of the root #ValentData context for Valent.
 *
 * Returns: (transfer none) (nullable): a #ValentData
 */
ValentData *
valent_data_get_parent (ValentData *data)
{
  ValentDataPrivate *priv = valent_data_get_instance_private (data);

  g_return_val_if_fail (VALENT_IS_DATA (data), NULL);

  return priv->parent;
}

/**
 * valent_data_clear_cache:
 * @data: a #ValentData
 *
 * Delete all cache data for @data.
 */
void
valent_data_clear_cache (ValentData *data)
{
  ValentDataPrivate *priv = valent_data_get_instance_private (data);
  g_autoptr (GError) error = NULL;
  g_autoptr (GFile) directory = NULL;

  g_return_if_fail (VALENT_IS_DATA (data));

  directory = g_file_new_for_path (priv->cache_path);

  if (!remove_directory (directory, NULL, &error))
    VALENT_NOTE ("Error deleting cache directory: %s", error->message);
}

/**
 * valent_data_clear_data:
 * @data: a #ValentData
 *
 * Delete all files in the cache, config and data directories for @data. This is
 * a no-op for the root #ValentData object, since it would wipe all data for all
 * contexts.
 */
void
valent_data_clear_data (ValentData *data)
{
  ValentDataPrivate *priv = valent_data_get_instance_private (data);
  g_autoptr (GError) error = NULL;
  g_autoptr (GFile) cache_dir = NULL;
  g_autoptr (GFile) config_dir = NULL;
  g_autoptr (GFile) data_dir = NULL;

  g_return_if_fail (VALENT_IS_DATA (data));

  /* We have to be careful not to remove device config directories */
  if (priv->context == NULL)
    return;

  cache_dir = g_file_new_for_path (priv->cache_path);

  if (!remove_directory (cache_dir, NULL, &error))
    {
      VALENT_NOTE ("%s", error->message);
      g_clear_error (&error);
    }

  config_dir = g_file_new_for_path (priv->config_path);

  if (!remove_directory (config_dir, NULL, &error))
    {
      VALENT_NOTE ("%s", error->message);
      g_clear_error (&error);
    }

  data_dir = g_file_new_for_path (priv->data_path);

  if (!remove_directory (data_dir, NULL, &error))
    {
      VALENT_NOTE ("%s", error->message);
      g_clear_error (&error);
    }
}

