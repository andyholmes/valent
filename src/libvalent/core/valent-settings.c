/* valent-settings.c
 *
 * Copyright 2024 Christian Hergert <chergert@redhat.com>
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#define G_LOG_DOMAIN "valent-settings"

#include "config.h"

#include <glib/gi18n.h>

#include <stdlib.h>

#define G_SETTINGS_ENABLE_BACKEND
#include <gio/gsettingsbackend.h>

#include "valent-data-source.h"
#include "valent-debug.h"
#include "valent-settings.h"

#define VALENT_SETTINGS_ROOT_GROUP "ca.andyholmes.valent"
#define VALENT_SETTINGS_ROOT_PATH  "/ca/andyholmes/valent/"

struct _ValentSettings
{
  GObject           parent_instance;

  ValentDataSource *data_source;
  char             *path;
  GSettingsSchema  *schema;
  char             *schema_id;

  GPtrArray        *layers;
  GSettings        *default_settings;
  GSettings        *memory_settings;
};

static GSettingsSchemaKey  * valent_settings_get_key   (ValentSettings *self,
                                                        const char     *key);
static char               ** valent_settings_list_keys (ValentSettings *self);

static void                  g_action_group_iface_init (GActionGroupInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (ValentSettings, valent_settings, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_ACTION_GROUP, g_action_group_iface_init))

typedef enum {
  PROP_DATA_SOURCE = 1,
  PROP_PATH,
  PROP_SCHEMA,
  PROP_SCHEMA_ID,
} ValentSettingsProperty;

static GParamSpec *properties[PROP_SCHEMA_ID + 1] = { NULL, };

typedef enum {
  CHANGED,
} ValentSettingsSignal;

static guint signals[CHANGED + 1] = { 0, };

/*
 * ValentSettings
 */
static void
valent_settings_cache_key (ValentSettings *self,
                           const char     *key)
{
  g_autoptr (GVariant) value = NULL;
  GSettings *settings;

  g_assert (VALENT_IS_SETTINGS (self));
  g_assert (key != NULL);
  g_assert (self->layers->len > 0);

  for (unsigned int i = 0; i < self->layers->len; i++)
    {
      settings = g_ptr_array_index (self->layers, i);
      value = g_settings_get_user_value (settings, key);

      if (value != NULL)
        {
          g_settings_set_value (self->memory_settings, key, value);
          goto emit_changed;
        }
    }

  settings = g_ptr_array_index (self->layers, 0);
  value = g_settings_get_value (settings, key);
  g_settings_set_value (self->memory_settings, key, value);

emit_changed:
  g_signal_emit (self, signals[CHANGED], g_quark_from_string (key), key);
  g_action_group_action_state_changed (G_ACTION_GROUP (self), key, value);
}

static void
valent_settings_update_cache (ValentSettings *self)
{
  g_auto (GStrv) keys = NULL;

  g_assert (VALENT_IS_SETTINGS (self));

  keys = g_settings_schema_list_keys (self->schema);
  for (unsigned int i = 0; keys != NULL && keys[i] != NULL; i++)
    valent_settings_cache_key (self, keys[i]);
}

static void
valent_settings_append (ValentSettings *self,
                        GSettings      *settings)
{
  g_autoptr (GSettingsSchema) settings_schema = NULL;
  g_auto (GStrv) keys = NULL;

  g_return_if_fail (VALENT_IS_SETTINGS (self));
  g_return_if_fail (G_IS_SETTINGS (settings));

  g_ptr_array_add (self->layers, g_object_ref (settings));

  /* Query all keys to ensure we get change notifications
   */
  g_object_get (settings, "settings-schema", &settings_schema, NULL);
  keys = g_settings_schema_list_keys (settings_schema);
  for (unsigned int i = 0; keys != NULL && keys[i] != NULL; i++)
    g_variant_unref (g_settings_get_value (settings, keys[i]));

  g_signal_connect_object (settings,
                           "changed",
                           G_CALLBACK (valent_settings_cache_key),
                           self,
                           G_CONNECT_SWAPPED);
  valent_settings_update_cache (self);
}

static GSettingsSchemaKey *
valent_settings_get_key (ValentSettings *self,
                         const char     *key)
{
  GSettingsSchemaKey *ret;

  g_assert (VALENT_IS_SETTINGS (self));
  g_assert (key != NULL);

  ret = g_settings_schema_get_key (self->schema, key);
  g_assert (ret != NULL);

  return ret;
}

static char **
valent_settings_list_keys (ValentSettings *self)
{
  g_return_val_if_fail (VALENT_IS_SETTINGS (self), NULL);

  return g_settings_schema_list_keys (self->schema);
}

static GSettings *
valent_settings_get_primary_settings (ValentSettings *self)
{
  g_assert (VALENT_IS_SETTINGS (self));

  if (self->layers->len == 0)
    g_critical ("No settings have been loaded. Aborting.");

  return g_ptr_array_index (self->layers, 0);
}

/*
 * GActionGroup
 */
static const GVariantType *
_g_variant_type_intern (const GVariantType *type)
{
  g_autofree char *str = NULL;

  if (type == NULL)
    return NULL;

  str = g_variant_type_dup_string (type);
  return G_VARIANT_TYPE (g_intern_string (str));
}

static gboolean
valent_settings_has_action (GActionGroup *group,
                            const char   *action_name)
{
  ValentSettings *self = VALENT_SETTINGS (group);
  g_auto (GStrv) keys = valent_settings_list_keys (self);

  return g_strv_contains ((const char * const *)keys, action_name);
}

static char **
valent_settings_list_actions (GActionGroup *group)
{
  ValentSettings *self = VALENT_SETTINGS (group);

  return valent_settings_list_keys (self);
}

static gboolean
valent_settings_get_action_enabled (GActionGroup *group,
                                    const char   *action_name)
{
  return TRUE;
}

static GVariant *
valent_settings_get_action_state (GActionGroup *group,
                                  const char   *action_name)
{
  ValentSettings *self = VALENT_SETTINGS (group);

  return valent_settings_get_value (self, action_name);
}

static GVariant *
valent_settings_get_action_state_hint (GActionGroup *group,
                                       const char   *action_name)
{
  ValentSettings *self = VALENT_SETTINGS (group);
  g_autoptr (GSettingsSchemaKey) key = valent_settings_get_key (self, action_name);

  return g_settings_schema_key_get_range (key);
}

static void
valent_settings_change_action_state (GActionGroup *group,
                                     const char   *action_name,
                                     GVariant     *value)
{
  ValentSettings *self = VALENT_SETTINGS (group);
  g_autoptr (GSettingsSchemaKey) key = valent_settings_get_key (self, action_name);

  if (g_variant_is_of_type (value, g_settings_schema_key_get_value_type (key)) &&
      g_settings_schema_key_range_check (key, value))
    {
      g_autoptr (GVariant) hold = g_variant_ref_sink (value);

      valent_settings_set_value (self, action_name, hold);
      g_action_group_action_state_changed (group, action_name, hold);
    }
}

static const GVariantType *
valent_settings_get_action_state_type (GActionGroup *group,
                                       const char   *action_name)
{
  ValentSettings *self = VALENT_SETTINGS (group);
  g_autoptr (GSettingsSchemaKey) key = valent_settings_get_key (self, action_name);
  const GVariantType *type = g_settings_schema_key_get_value_type (key);

  return _g_variant_type_intern (type);
}

static void
valent_settings_activate_action (GActionGroup *group,
                                 const char   *action_name,
                                 GVariant     *parameter)
{
  ValentSettings *self = VALENT_SETTINGS (group);
  g_autoptr (GSettingsSchemaKey) key = valent_settings_get_key (self, action_name);
  g_autoptr (GVariant) default_value = g_settings_schema_key_get_default_value (key);

  if (g_variant_is_of_type (default_value, G_VARIANT_TYPE_BOOLEAN))
    {
      GVariant *old;

      if (parameter != NULL)
        return;

      old = valent_settings_get_action_state (group, action_name);
      parameter = g_variant_new_boolean (!g_variant_get_boolean (old));
      g_variant_unref (old);
    }

  g_action_group_change_action_state (group, action_name, parameter);
}

static const GVariantType *
valent_settings_get_action_parameter_type (GActionGroup *group,
                                           const char   *action_name)
{
  ValentSettings *self = VALENT_SETTINGS (group);
  g_autoptr (GSettingsSchemaKey) key = valent_settings_get_key (self, action_name);
  const GVariantType *type = g_settings_schema_key_get_value_type (key);

  if (g_variant_type_equal (type, G_VARIANT_TYPE_BOOLEAN))
    return NULL;

  return _g_variant_type_intern (type);
}

static void
g_action_group_iface_init (GActionGroupInterface *iface)
{
  iface->has_action = valent_settings_has_action;
  iface->list_actions = valent_settings_list_actions;
  iface->get_action_parameter_type = valent_settings_get_action_parameter_type;
  iface->get_action_enabled = valent_settings_get_action_enabled;
  iface->get_action_state = valent_settings_get_action_state;
  iface->get_action_state_hint = valent_settings_get_action_state_hint;
  iface->get_action_state_type = valent_settings_get_action_state_type;
  iface->change_action_state = valent_settings_change_action_state;
  iface->activate_action = valent_settings_activate_action;
}

/*
 * GObject
 */
static void
valent_settings_constructed (GObject *object)
{
  ValentSettings *self = VALENT_SETTINGS (object);
  ValentDataSource *parent_source;
  g_autoptr (GSettingsBackend) memory_backend = NULL;
  GSettingsBackend *source_backend;
  GSettings *source_settings;

  G_OBJECT_CLASS (valent_settings_parent_class)->constructed (object);

  if (self->schema == NULL)
    {
      GSettingsSchemaSource *schema_source = NULL;

      if (self->schema_id == NULL)
        {
          g_critical ("Either %s:schema or %s:schema-id must be set",
                      G_OBJECT_TYPE_NAME (self),
                      G_OBJECT_TYPE_NAME (self));
          return;
        }

      schema_source = g_settings_schema_source_get_default ();
      self->schema = g_settings_schema_source_lookup (schema_source,
                                                      self->schema_id,
                                                      TRUE);

      if (self->schema == NULL)
        {
          g_critical ("Failed to locate GSettings schema %s", self->schema_id);
          return;
        }
    }

  if (self->path == NULL)
    self->path = g_strdup (g_settings_schema_get_path (self->schema));

  memory_backend = g_memory_settings_backend_new ();
  self->memory_settings = g_settings_new_full (self->schema,
                                               memory_backend,
                                               self->path);

  /* Primary settings backend
   */
  source_backend = valent_data_source_get_settings_backend (self->data_source);
  source_settings = g_settings_new_full (self->schema,
                                         source_backend,
                                         self->path);
  valent_settings_append (self, source_settings);

  /* If the data source has a parent, it serves as the fallback backend, which
   * will chain-up to any ancestors.
   */
  parent_source = valent_resource_get_ancestor (VALENT_RESOURCE (self->data_source),
                                                VALENT_TYPE_DATA_SOURCE);
  if (parent_source != NULL)
    {
      GSettingsBackend *parent_backend;
      g_autoptr (GSettings) parent_settings = NULL;

      parent_backend = valent_data_source_get_settings_backend (parent_source);
      parent_settings = g_settings_new_full (self->schema,
                                             parent_backend,
                                             self->path);
      valent_settings_append (self, parent_settings);
    }
}

static void
valent_settings_finalize (GObject *object)
{
  ValentSettings *self = VALENT_SETTINGS (object);

  g_clear_object (&self->default_settings);
  g_clear_object (&self->memory_settings);
  g_clear_pointer (&self->layers, g_ptr_array_unref);

  g_clear_object (&self->data_source);
  g_clear_pointer (&self->path, g_free);
  g_clear_pointer (&self->schema, g_settings_schema_unref);
  g_clear_pointer (&self->schema_id, g_free);

  G_OBJECT_CLASS (valent_settings_parent_class)->finalize (object);
}

static void
valent_settings_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  ValentSettings *self = VALENT_SETTINGS (object);

  switch ((ValentSettingsProperty)prop_id)
    {
    case PROP_DATA_SOURCE:
      g_value_set_object (value, self->data_source);
      break;

    case PROP_PATH:
      g_value_set_string (value, self->path);
      break;

    case PROP_SCHEMA:
      g_value_set_boxed (value, self->schema);
      break;

    case PROP_SCHEMA_ID:
      g_value_set_string (value, self->schema_id);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_settings_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  ValentSettings *self = VALENT_SETTINGS (object);

  switch ((ValentSettingsProperty)prop_id)
    {
    case PROP_DATA_SOURCE:
      self->data_source = g_value_dup_object (value);
      break;

    case PROP_PATH:
      self->path = g_value_dup_string (value);
      break;

    case PROP_SCHEMA:
      self->schema = g_value_dup_boxed (value);
      break;

    case PROP_SCHEMA_ID:
      self->schema_id = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_settings_class_init (ValentSettingsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = valent_settings_constructed;
  object_class->finalize = valent_settings_finalize;
  object_class->get_property = valent_settings_get_property;
  object_class->set_property = valent_settings_set_property;

  /**
   * ValentSettings:data-source:
   *
   * The [class@Valent.DataSource] providing the backend.
   *
   * Since: 1.0
   */
  properties [PROP_DATA_SOURCE] =
    g_param_spec_object ("data-source", NULL, NULL,
                         VALENT_TYPE_DATA_SOURCE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentSettings:path:
   *
   * The path within the backend where the settings are stored.
   *
   * Since: 1.0
   */
  properties[PROP_PATH] =
    g_param_spec_string ("path", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentSettings:schema:
   *
   * The [type@Gio.SettingsSchema].
   *
   * Since: 1.0
   */
  properties[PROP_SCHEMA] =
    g_param_spec_boxed ("schema", NULL, NULL,
                        G_TYPE_SETTINGS_SCHEMA,
                        (G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS));

  /**
   * ValentSettings:schema-id:
   *
   * The [type@Gio.SettingsSchema] ID.
   *
   * Since: 1.0
   */
  properties[PROP_SCHEMA_ID] =
    g_param_spec_string ("schema-id", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, G_N_ELEMENTS (properties), properties);

  /**
   * ValentSettings::changed:
   * @service: a `ValentSettings`
   * @key: a key name
   *
   * The “changed” signal is emitted when a key has potentially changed. You
   * should call [method@Valent.Settings.get_value] to check the new value.
   *
   * You can connect to the detailed signal (e.g. `changed::key-name`) to
   * receive notifications for a single key.
   *
   * Since: 1.0
   */
  signals[CHANGED] =
    g_signal_new ("changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__STRING,
                  G_TYPE_NONE, 1,  G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE);
  g_signal_set_va_marshaller (signals [CHANGED],
                              G_TYPE_FROM_CLASS (klass),
                              g_cclosure_marshal_VOID__STRINGv);
}

static void
valent_settings_init (ValentSettings *self)
{
  self->layers = g_ptr_array_new_with_free_func (g_object_unref);
}

/**
 * valent_settings_new:
 * @data_source: a `ValentDataSource`
 * @schema_id: the a `GSettingsSchema`
 *
 * Create a new [class@Valent.Settings] for @data_source and @schema_id.
 *
 * Returns: (transfer full): a `ValentSettings`
 *
 * Since: 1.0
 */
ValentSettings *
valent_settings_new (ValentDataSource *data_source,
                     const char       *schema_id)
{
  ValentSettings *ret;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_DATA_SOURCE (data_source), NULL);
  g_return_val_if_fail (schema_id != NULL, NULL);

  ret = g_object_new (VALENT_TYPE_SETTINGS,
                      "data-source", data_source,
                      "schema-id",   schema_id,
                      NULL);

  VALENT_RETURN (ret);
}

/**
 * valent_settings_new_full:
 * @data_source: a `ValentDataSource`
 * @schema: the a `GSettingsSchema`
 * @path: (nullable): a `GSettings` path
 *
 * Create a new [class@Valent.Settings] for @data_source and @schema.
 *
 * Returns: (transfer full): a `ValentSettings`
 *
 * Since: 1.0
 */
ValentSettings *
valent_settings_new_full (ValentDataSource *data_source,
                          GSettingsSchema  *schema,
                          const char       *path)
{
  ValentSettings *ret;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_DATA_SOURCE (data_source), NULL);
  g_return_val_if_fail (schema != NULL, NULL);

  ret = g_object_new (VALENT_TYPE_SETTINGS,
                      "data-source", data_source,
                      "schema",      schema,
                      "path",        path,
                      NULL);

  VALENT_RETURN (ret);
}

/**
 * valent_settings_get_default_value:
 * @settings: a `ValentSettings`
 * @key: the key to get the value for
 *
 * Get the default value of @key.
 *
 * Returns: (transfer full): a `GVariant`
 *
 * Since: 1.0
 */
GVariant *
valent_settings_get_default_value (ValentSettings *settings,
                                   const char     *key)
{
  g_return_val_if_fail (VALENT_IS_SETTINGS (settings), NULL);
  g_return_val_if_fail (key != NULL, NULL);

  return g_settings_get_default_value (valent_settings_get_primary_settings (settings), key);
}

/**
 * valent_settings_get_user_value:
 * @settings: a `ValentSettings`
 * @key: the key to get the value for
 *
 * Get the value of @key from the first layer that is modified.
 *
 * If @key is unset, %NULL is returned.
 *
 * Returns: (transfer full) (nullable): a `GVariant`, or %NULL if unset
 *
 * Since: 1.0
 */
GVariant *
valent_settings_get_user_value (ValentSettings *settings,
                                const char     *key)
{
  g_return_val_if_fail (VALENT_IS_SETTINGS (settings), NULL);
  g_return_val_if_fail (key != NULL, NULL);

  for (unsigned int i = 0; i < settings->layers->len; i++)
    {
      GSettings *layer = g_ptr_array_index (settings->layers, i);
      GVariant *value = g_settings_get_user_value (layer, key);

      if (value != NULL)
        return g_steal_pointer (&value);
    }

  return NULL;
}

/**
 * valent_settings_get_value:
 * @settings: a `ValentSettings`
 * @key: the key to get the value for
 *
 * Get the value of @key from the first layer that is modified.
 *
 * If @key is unset, the default value from the primary settings is returned.
 *
 * Returns: (transfer full): a `GVariant`
 *
 * Since: 1.0
 */
GVariant *
valent_settings_get_value (ValentSettings *settings,
                           const char     *key)
{
  g_return_val_if_fail (VALENT_IS_SETTINGS (settings), NULL);
  g_return_val_if_fail (key != NULL, NULL);

  for (unsigned int i = 0; i < settings->layers->len; i++)
    {
      GSettings *layer = g_ptr_array_index (settings->layers, i);
      GVariant *value = g_settings_get_user_value (layer, key);

      if (value != NULL)
        return g_steal_pointer (&value);
    }

  return g_settings_get_value (valent_settings_get_primary_settings (settings), key);
}

/**
 * valent_settings_set_value:
 * @settings: a `ValentSettings`
 * @key: the key to set the value for
 * @value: a `GVariant`
 *
 * Set the value of @key from the first layer that is modified.
 *
 * Since: 1.0
 */
void
valent_settings_set_value (ValentSettings *settings,
                           const char     *key,
                           GVariant       *value)
{
  g_return_if_fail (VALENT_IS_SETTINGS (settings));
  g_return_if_fail (key != NULL);

  g_settings_set_value (valent_settings_get_primary_settings (settings),
                        key, value);
  valent_settings_cache_key (settings, key);
}

/**
 * valent_settings_get_boolean:
 * @settings: a `ValentSettings`
 * @key: the key to get the value for
 *
 * Gets the value of @key from the first layer that is modified.
 *
 * Returns: %TRUE or %FALSE
 *
 * Since: 1.0
 */
gboolean
valent_settings_get_boolean (ValentSettings *settings,
                             const char     *key)
{
  g_autoptr (GVariant) value = NULL;

  g_return_val_if_fail (VALENT_IS_SETTINGS (settings), FALSE);
  g_return_val_if_fail (key != NULL, FALSE);

  value = valent_settings_get_value (settings, key);
  return g_variant_get_boolean (value);
}

/**
 * valent_settings_set_boolean:
 * @settings: a `ValentSettings`
 * @key: the key to set the value for
 * @val: the new value
 *
 * Sets the value of @key in the primary settings to @val.
 *
 * Since: 1.0
 */
void
valent_settings_set_boolean (ValentSettings *settings,
                             const char     *key,
                             gboolean        val)
{
  GVariant *value = NULL;

  g_return_if_fail (VALENT_IS_SETTINGS (settings));
  g_return_if_fail (key != NULL);

  value = g_variant_new_boolean (val);
  valent_settings_set_value (settings, key, value);
}

/**
 * valent_settings_get_double:
 * @settings: a `ValentSettings`
 * @key: the key to get the value for
 *
 * Gets the value of @key from the first layer that is modified.
 *
 * Returns: a double-precision float
 *
 * Since: 1.0
 */
double
valent_settings_get_double (ValentSettings *settings,
                            const char     *key)
{
  g_autoptr (GVariant) value = NULL;

  g_return_val_if_fail (VALENT_IS_SETTINGS (settings), FALSE);
  g_return_val_if_fail (key != NULL, FALSE);

  value = valent_settings_get_value (settings, key);
  return g_variant_get_double (value);
}

/**
 * valent_settings_set_double:
 * @settings: a `ValentSettings`
 * @key: the key to set the value for
 * @val: the new value
 *
 * Sets the value of @key in the primary settings to @val.
 *
 * Since: 1.0
 */
void
valent_settings_set_double (ValentSettings *settings,
                            const char     *key,
                            double          val)
{
  g_return_if_fail (VALENT_IS_SETTINGS (settings));
  g_return_if_fail (key != NULL);

  valent_settings_set_value (settings, key, g_variant_new_double (val));
}

/**
 * valent_settings_get_int32:
 * @settings: a `ValentSettings`
 * @key: the key to get the value for
 *
 * Gets the value of @key from the first layer that is modified.
 *
 * Returns: a 32-bit signed integer
 *
 * Since: 1.0
 */
int32_t
valent_settings_get_int32 (ValentSettings *settings,
                           const char     *key)
{
  g_autoptr (GVariant) value = NULL;

  g_return_val_if_fail (VALENT_IS_SETTINGS (settings), FALSE);
  g_return_val_if_fail (key != NULL, FALSE);

  value = valent_settings_get_value (settings, key);
  return g_variant_get_int32 (value);
}

/**
 * valent_settings_set_int32:
 * @settings: a `ValentSettings`
 * @key: the key to set the value for
 * @val: the new value
 *
 * Sets the value of @key in the primary settings to @val.
 *
 * Since: 1.0
 */
void
valent_settings_set_int32 (ValentSettings *settings,
                           const char     *key,
                           int32_t         val)
{
  g_return_if_fail (VALENT_IS_SETTINGS (settings));
  g_return_if_fail (key != NULL);

  valent_settings_set_value (settings, key, g_variant_new_int32 (val));
}


/**
 * valent_settings_get_int64:
 * @settings: a `ValentSettings`
 * @key: the key to get the value for
 *
 * Gets the value of @key from the first layer that is modified.
 *
 * Returns: a 64-bit signed integer
 *
 * Since: 1.0
 */
int64_t
valent_settings_get_int64 (ValentSettings *settings,
                           const char     *key)
{
  g_autoptr (GVariant) value = NULL;

  g_return_val_if_fail (VALENT_IS_SETTINGS (settings), FALSE);
  g_return_val_if_fail (key != NULL, FALSE);

  value = valent_settings_get_value (settings, key);
  return g_variant_get_int64 (value);
}

/**
 * valent_settings_set_int64:
 * @settings: a `ValentSettings`
 * @key: the key to set the value for
 * @val: the new value
 *
 * Sets the value of @key in the primary settings to @val.
 *
 * Since: 1.0
 */
void
valent_settings_set_int64 (ValentSettings *settings,
                           const char     *key,
                           int64_t         val)
{
  g_return_if_fail (VALENT_IS_SETTINGS (settings));
  g_return_if_fail (key != NULL);

  valent_settings_set_value (settings, key, g_variant_new_int64 (val));
}

/**
 * valent_settings_get_string:
 * @settings: a `ValentSettings`
 * @key: the key to get the value for
 *
 * Gets the value of @key from the first layer that is modified.
 *
 * Returns: (transfer full) (not nullable): a new string
 *
 * Since: 1.0
 */
char *
valent_settings_get_string (ValentSettings *settings,
                            const char     *key)
{
  g_autoptr (GVariant) value = NULL;

  g_return_val_if_fail (VALENT_IS_SETTINGS (settings), FALSE);
  g_return_val_if_fail (key != NULL, FALSE);

  value = valent_settings_get_value (settings, key);
  return g_variant_dup_string (value, NULL);
}

/**
 * valent_settings_set_string:
 * @settings: a `ValentSettings`
 * @key: the key to set the value for
 * @val: the new value
 *
 * Sets the value of @key in the primary settings to @val.
 *
 * Since: 1.0
 */
void
valent_settings_set_string (ValentSettings *settings,
                            const char     *key,
                            const char     *val)
{
  g_return_if_fail (VALENT_IS_SETTINGS (settings));
  g_return_if_fail (key != NULL);
  g_return_if_fail (val != NULL);

  valent_settings_set_value (settings, key, g_variant_new_string (val));
}

/**
 * valent_settings_get_strv:
 * @settings: a #GSettings object
 * @key: the key to get the value for
 *
 * Gets the value of @key from the first layer that is modified.
 *
 * Returns: (array zero-terminated=1) (not nullable) (transfer full): a
 * newly-allocated, %NULL-terminated array of strings, the value that
 * is stored at @key in @settings.
 *
 * Since: 1.0
 */
char **
valent_settings_get_strv (ValentSettings *settings,
                          const char     *key)
{
  g_autoptr (GVariant) value = NULL;

  g_return_val_if_fail (VALENT_IS_SETTINGS (settings), FALSE);
  g_return_val_if_fail (key != NULL, FALSE);

  value = valent_settings_get_value (settings, key);
  return g_variant_dup_strv (value, NULL);
}

/**
 * valent_settings_set_strv:
 * @settings: a `ValentSettings`
 * @key: the key to set the value for
 * @value: (nullable) (array zero-terminated=1): the value to set it to, or %NULL
 *
 * Sets the value of @key in the primary settings to @val.
 *
 * Since: 1.0
 */
void
valent_settings_set_strv (ValentSettings     *settings,
                          const char         *key,
                          const char * const *value)
{
  GVariant *array;

  if (value != NULL)
    array = g_variant_new_strv (value, -1);
  else
    array = g_variant_new_strv (NULL, 0);

  valent_settings_set_value (settings, key, array);
}

/**
 * valent_settings_get_uint32:
 * @settings: a `ValentSettings`
 * @key: the key to get the value for
 *
 * Gets the value of @key from the first layer that is modified.
 *
 * Returns: a 32-bit unsigned integer
 *
 * Since: 1.0
 */
uint32_t
valent_settings_get_uint32 (ValentSettings *settings,
                            const char     *key)
{
  g_autoptr (GVariant) value = NULL;

  g_return_val_if_fail (VALENT_IS_SETTINGS (settings), FALSE);
  g_return_val_if_fail (key != NULL, FALSE);

  value = valent_settings_get_value (settings, key);
  return g_variant_get_uint32 (value);
}

/**
 * valent_settings_set_uint32:
 * @settings: a `ValentSettings`
 * @key: the key to set the value for
 * @val: the new value
 *
 * Sets the value of @key in the primary settings to @val.
 *
 * Since: 1.0
 */
void
valent_settings_set_uint32 (ValentSettings *settings,
                            const char     *key,
                            uint32_t        val)
{
  g_return_if_fail (VALENT_IS_SETTINGS (settings));
  g_return_if_fail (key != NULL);

  valent_settings_set_value (settings, key, g_variant_new_uint32 (val));
}

/**
 * valent_settings_get_uint64:
 * @settings: a `ValentSettings`
 * @key: the key to get the value for
 *
 * Gets the value of @key from the first layer that is modified.
 *
 * Returns: a 64-bit unsigned integer
 *
 * Since: 1.0
 */
uint64_t
valent_settings_get_uint64 (ValentSettings *settings,
                            const char     *key)
{
  g_autoptr (GVariant) value = NULL;

  g_return_val_if_fail (VALENT_IS_SETTINGS (settings), FALSE);
  g_return_val_if_fail (key != NULL, FALSE);

  value = valent_settings_get_value (settings, key);
  return g_variant_get_uint64 (value);
}

/**
 * valent_settings_set_uint64:
 * @settings: a `ValentSettings`
 * @key: the key to set the value for
 * @val: the new value
 *
 * Sets the value of @key in the primary settings to @val.
 *
 * Since: 1.0
 */
void
valent_settings_set_uint64 (ValentSettings *settings,
                            const char     *key,
                            uint64_t        val)
{
  g_return_if_fail (VALENT_IS_SETTINGS (settings));
  g_return_if_fail (key != NULL);

  valent_settings_set_value (settings, key, g_variant_new_uint64 (val));
}

/**
 * valent_settings_reset:
 * @settings: a `ValentSettings`
 * @key: the key to set the value for
 *
 * Reset the value of @key in the primary settings.
 *
 * Since: 1.0
 */
void
valent_settings_reset (ValentSettings *settings,
                       const char     *key)
{
  g_return_if_fail (VALENT_IS_SETTINGS (settings));
  g_return_if_fail (key != NULL);

  g_settings_reset (valent_settings_get_primary_settings (settings), key);
}

static gboolean
get_inverted_boolean (GValue   *value,
                      GVariant *variant,
                      gpointer  user_data)
{
  if (g_variant_is_of_type (variant, G_VARIANT_TYPE_BOOLEAN))
    {
      g_value_set_boolean (value, !g_variant_get_boolean (variant));
      return TRUE;
    }

  return FALSE;
}

static GVariant *
set_inverted_boolean (const GValue       *value,
                      const GVariantType *type,
                      gpointer            user_data)
{
  if (G_VALUE_HOLDS (value, G_TYPE_BOOLEAN) &&
      g_variant_type_equal (type, G_VARIANT_TYPE_BOOLEAN))
    return g_variant_new_boolean (!g_value_get_boolean (value));

  return NULL;
}

/**
 * valent_settings_bind:
 * @settings: An `ValentSettings`
 * @key: The settings key
 * @object: the object to bind to
 * @property: the property of @object to bind to
 * @flags: flags for the binding
 *
 * A wrapper around [method@Gio.Settings.bind].
 *
 * Call [method@Valent.Settings.unbind] to remove the mapping.
 *
 * Since: 1.0
 */
void
valent_settings_bind (ValentSettings     *settings,
                      const char         *key,
                      gpointer            object,
                      const char         *property,
                      GSettingsBindFlags  flags)
{
  GSettingsBindGetMapping get_mapping = NULL;
  GSettingsBindSetMapping set_mapping = NULL;

  g_return_if_fail (VALENT_IS_SETTINGS (settings));
  g_return_if_fail (key != NULL);
  g_return_if_fail (G_IS_OBJECT (object));
  g_return_if_fail (property != NULL);

  if (flags & G_SETTINGS_BIND_INVERT_BOOLEAN)
    {
      flags &= ~G_SETTINGS_BIND_INVERT_BOOLEAN;
      get_mapping = get_inverted_boolean;
      set_mapping = set_inverted_boolean;
    }

  valent_settings_bind_with_mapping (settings, key, object, property, flags,
                                     get_mapping, set_mapping, NULL, NULL);
}

/**
 * valent_settings_bind_with_mapping:
 * @settings: An `ValentSettings`
 * @key: The settings key
 * @object: the object to bind to
 * @property: the property of @object to bind to
 * @flags: flags for the binding
 * @get_mapping: (allow-none) (scope notified): variant to value mapping
 * @set_mapping: (allow-none) (scope notified): value to variant mapping
 * @user_data: user data for @get_mapping and @set_mapping
 * @destroy: destroy function to cleanup @user_data.
 *
 * A wrapper around [method@Gio.Settings.bind_with_mapping].
 *
 * Call [method@Valent.Settings.unbind] to remove the mapping.
 *
 * Since: 1.0
 */
void
valent_settings_bind_with_mapping (ValentSettings          *settings,
                                   const char              *key,
                                   gpointer                 object,
                                   const char              *property,
                                   GSettingsBindFlags       flags,
                                   GSettingsBindGetMapping  get_mapping,
                                   GSettingsBindSetMapping  set_mapping,
                                   gpointer                 user_data,
                                   GDestroyNotify           destroy)
{
  static const GSettingsBindFlags default_flags = G_SETTINGS_BIND_GET|G_SETTINGS_BIND_SET;
  GDestroyNotify get_destroy = destroy;
  GDestroyNotify set_destroy = destroy;

  g_return_if_fail (VALENT_IS_SETTINGS (settings));
  g_return_if_fail (key != NULL);
  g_return_if_fail (G_IS_OBJECT (object));
  g_return_if_fail (property != NULL);

  /* Make sure we have GET|SET flags if DEFAULT was specified
   */
  if ((flags & default_flags) == 0)
    flags |= default_flags;

  /* Ensure @destroy is only called once, on the longer living
   * setting binding (potential for longer living that is).
   */
  if ((flags & G_SETTINGS_BIND_SET) != 0)
    get_destroy = NULL;
  else
    set_destroy = NULL;

  /* Our memory backend/settings are compiling the values from all of the
   * layers. Therefore, we only want to map reads from the memory backend. We
   * want to direct all writes to the topmost layer (found at index 0).
   */
  if ((flags & G_SETTINGS_BIND_GET) != 0)
    g_settings_bind_with_mapping (settings->memory_settings, key, object, property,
                                  (flags & ~G_SETTINGS_BIND_SET),
                                  get_mapping, NULL, user_data, get_destroy);

  /* We bind writability directly to our toplevel layer.
   */
  if ((flags & G_SETTINGS_BIND_SET) != 0)
    g_settings_bind_with_mapping (valent_settings_get_primary_settings (settings),
                                  key, object, property, (flags & ~G_SETTINGS_BIND_GET),
                                  NULL, set_mapping, user_data, set_destroy);
}

/**
 * valent_settings_unbind:
 * @settings: An `ValentSettings`
 * @property: the property bound to @object to bind to
 *
 * Unbind a mapping.
 *
 * Since: 1.0
 */
void
valent_settings_unbind (ValentSettings *settings,
                        const char     *property)
{
  g_return_if_fail (VALENT_IS_SETTINGS (settings));
  g_return_if_fail (property != NULL);

  g_settings_unbind (valent_settings_get_primary_settings (settings), property);
  g_settings_unbind (settings->memory_settings, property);
}

