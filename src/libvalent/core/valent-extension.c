// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-extension"

#include "config.h"

#include <gio/gio.h>
#include <libpeas.h>

#include "valent-core-enums.h"
#include "valent-data-source.h"
#include "valent-object.h"
#include "valent-resource.h"

#include "valent-extension.h"


/**
 * ValentExtension:
 *
 * An abstract base class for extensions.
 *
 * `ValentExtension` is a base class for extensions in Valent, with conveniences
 * for [iface@Gio.Action] and [class@Gio.Settings].
 *
 * ## Implementation Notes
 *
 * Implementations that also implement [iface@Gio.Initable] or
 * [iface@Gio.AsyncInitable] are marked as `VALENT_PLUGIN_STATE_INACTIVE` during
 * construction and must call [method@Valent.Extension.plugin_state_changed] to
 * reflect the result of initialization.
 *
 * ## Plugin Actions
 *
 * `ValentExtension` implements the [iface@Gio.ActionGroup] and
 * [iface@Gio.ActionMap] interfaces, providing a simple way for plugins to
 * expose functions and states. Each [iface@Gio.Action] added to the action map
 * will be included in the object action group with the plugin's module name as
 * a prefix (eg. `share.uri`).
 *
 * ## `.plugin` File
 *
 * Implementations may define the extra fields in the `.plugin` file, to take
 * advantage of core features in the base class.
 *
 * The field names are inferred from the GType name of the implementation, with
 * `Valent` being stripped if present. For example `ValentDevicePlugin` becomes
 * `X-DevicePluginSettings`, while `NameDevicePlugin` would become
 * `X-NameDevicePluginSettings`.
 *
 * - Extension Category Field
 *
 *     A list of categories separated by semi-colons, serving as a hint for
 *     organizational purposes. This should be in the form `Main;Additional;`,
 *     with values from the freedesktop.org Desktop Menu Specification.
 *
 *     Field pattern: `X-<type name>Category`
 *
 * - [class@Gio.Settings] Schema Field
 *
 *     A [class@Gio.Settings] schema ID for the extensions's settings. See
 *     [method@Valent.DataSource.get_plugin_settings] for more information.
 *
 *     Field pattern: `X-<type name>Settings`
 *
 * Since: 1.0
 */

typedef struct
{
  PeasPluginInfo    *plugin_info;
  char              *plugin_domain;
  ValentPluginState  plugin_state;
  GError            *plugin_error;

  GHashTable        *actions;
  GSettings         *settings;
} ValentExtensionPrivate;

static void   g_action_group_iface_init (GActionGroupInterface *iface);
static void   g_action_map_iface_init   (GActionMapInterface   *iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (ValentExtension, valent_extension, VALENT_TYPE_RESOURCE,
                                  G_ADD_PRIVATE (ValentExtension)
                                  G_IMPLEMENT_INTERFACE (G_TYPE_ACTION_GROUP, g_action_group_iface_init)
                                  G_IMPLEMENT_INTERFACE (G_TYPE_ACTION_MAP, g_action_map_iface_init))

typedef enum {
  PROP_PLUGIN_DOMAIN = 1,
  PROP_PLUGIN_INFO,
  PROP_PLUGIN_STATE,
  PROP_SETTINGS,
} ValentExtensionProperty;

static GParamSpec *properties[PROP_SETTINGS + 1] = { NULL, };


/*
 * GActionGroup
 */
static void
valent_extension_activate_action (GActionGroup *action_group,
                                  const char   *action_name,
                                  GVariant     *parameter)
{
  ValentExtension *self = VALENT_EXTENSION (action_group);
  ValentExtensionPrivate *priv = valent_extension_get_instance_private (self);
  GAction *action;

  if ((action = g_hash_table_lookup (priv->actions, action_name)) != NULL)
    g_action_activate (action, parameter);
}

static void
valent_extension_change_action_state (GActionGroup *action_group,
                                      const char   *action_name,
                                      GVariant     *value)
{
  ValentExtension *self = VALENT_EXTENSION (action_group);
  ValentExtensionPrivate *priv = valent_extension_get_instance_private (self);
  GAction *action;

  if ((action = g_hash_table_lookup (priv->actions, action_name)) != NULL)
    g_action_change_state (action, value);
}

static char **
valent_extension_list_actions (GActionGroup *action_group)
{
  ValentExtension *self = VALENT_EXTENSION (action_group);
  ValentExtensionPrivate *priv = valent_extension_get_instance_private (self);
  g_auto (GStrv) actions = NULL;
  GHashTableIter iter;
  gpointer key;
  unsigned int i = 0;

  actions = g_new0 (char *, g_hash_table_size (priv->actions) + 1);

  g_hash_table_iter_init (&iter, priv->actions);

  while (g_hash_table_iter_next (&iter, &key, NULL))
    actions[i++] = g_strdup (key);

  return g_steal_pointer (&actions);
}

static gboolean
valent_extension_query_action (GActionGroup        *action_group,
                               const char          *action_name,
                               gboolean            *enabled,
                               const GVariantType **parameter_type,
                               const GVariantType **state_type,
                               GVariant           **state_hint,
                               GVariant           **state)
{
  ValentExtension *self = VALENT_EXTENSION (action_group);
  ValentExtensionPrivate *priv = valent_extension_get_instance_private (self);
  GAction *action;

  if ((action = g_hash_table_lookup (priv->actions, action_name)) == NULL)
    return FALSE;

  if (enabled)
    *enabled = g_action_get_enabled (action);

  if (parameter_type)
    *parameter_type = g_action_get_parameter_type (action);

  if (state_type)
    *state_type = g_action_get_state_type (action);

  if (state_hint)
    *state_hint = g_action_get_state_hint (action);

  if (state)
    *state = g_action_get_state (action);

  return TRUE;
}

static void
g_action_group_iface_init (GActionGroupInterface *iface)
{
  iface->activate_action = valent_extension_activate_action;
  iface->change_action_state = valent_extension_change_action_state;
  iface->list_actions = valent_extension_list_actions;
  iface->query_action = valent_extension_query_action;
}

/*
 * GActionMap
 */
static void
on_action_enabled_changed (GAction      *action,
                           GParamSpec   *pspec,
                           GActionGroup *action_group)
{
  g_action_group_action_enabled_changed (action_group,
                                         g_action_get_name (action),
                                         g_action_get_enabled (action));
}

static void
on_action_state_changed (GAction      *action,
                         GParamSpec   *pspec,
                         GActionGroup *action_group)
{
  g_autoptr (GVariant) value = NULL;

  value = g_action_get_state (action);
  g_action_group_action_state_changed (action_group,
                                       g_action_get_name (action),
                                       value);
}

static void
valent_extension_disconnect_action (ValentExtension *self,
                                    GAction         *action)
{
  g_signal_handlers_disconnect_by_func (action, on_action_enabled_changed, self);
  g_signal_handlers_disconnect_by_func (action, on_action_state_changed, self);
}

static GAction *
valent_extension_lookup_action (GActionMap *action_map,
                                const char *action_name)
{
  ValentExtension *self = VALENT_EXTENSION (action_map);
  ValentExtensionPrivate *priv = valent_extension_get_instance_private (self);

  return g_hash_table_lookup (priv->actions, action_name);
}

static void
valent_extension_add_action (GActionMap *action_map,
                             GAction    *action)
{
  ValentExtension *self = VALENT_EXTENSION (action_map);
  ValentExtensionPrivate *priv = valent_extension_get_instance_private (self);
  const char *action_name;
  GAction *replacing;

  action_name = g_action_get_name (action);

  if ((replacing = g_hash_table_lookup (priv->actions, action_name)) == action)
    return;

  if (replacing != NULL)
    {
      g_action_group_action_removed (G_ACTION_GROUP (action_map), action_name);
      valent_extension_disconnect_action (self, replacing);
    }

  g_signal_connect_object (action,
                           "notify::enabled",
                           G_CALLBACK (on_action_enabled_changed),
                           action_map, 0);

  if (g_action_get_state_type (action) != NULL)
    g_signal_connect_object (action,
                             "notify::state",
                             G_CALLBACK (on_action_state_changed),
                             action_map, 0);

  g_hash_table_replace (priv->actions,
                        g_strdup (action_name),
                        g_object_ref (action));
  g_action_group_action_added (G_ACTION_GROUP (action_map), action_name);
}

static void
valent_extension_remove_action (GActionMap *action_map,
                                const char *action_name)
{
  ValentExtension *self = VALENT_EXTENSION (action_map);
  ValentExtensionPrivate *priv = valent_extension_get_instance_private (self);
  GAction *action;

  if ((action = g_hash_table_lookup (priv->actions, action_name)) != NULL)
    {
      g_action_group_action_removed (G_ACTION_GROUP (action_map), action_name);
      valent_extension_disconnect_action (self, action);
      g_hash_table_remove (priv->actions, action_name);
    }
}

static void
g_action_map_iface_init (GActionMapInterface *iface)
{
  iface->add_action = valent_extension_add_action;
  iface->lookup_action = valent_extension_lookup_action;
  iface->remove_action = valent_extension_remove_action;
}

/*
 * ValentObject
 */
static void
valent_extension_destroy (ValentObject *object)
{
  ValentExtension *self = VALENT_EXTENSION (object);
  ValentExtensionPrivate *priv = valent_extension_get_instance_private (self);
  GHashTableIter iter;
  const char *action_name;
  GAction *action;

  g_hash_table_iter_init (&iter, priv->actions);
  while (g_hash_table_iter_next (&iter, (void **)&action_name, (void **)&action))
    {
      g_action_group_action_removed (G_ACTION_GROUP (self), action_name);
      valent_extension_disconnect_action (self, action);
      g_hash_table_iter_remove (&iter);
    }

  valent_extension_plugin_state_changed (self, VALENT_PLUGIN_STATE_INACTIVE, NULL);

  VALENT_OBJECT_CLASS (valent_extension_parent_class)->destroy (object);
}

/*
 * GObject
 */
static void
valent_extension_constructed (GObject *object)
{
  ValentExtension *self = VALENT_EXTENSION (object);
  ValentExtensionPrivate *priv = valent_extension_get_instance_private (self);

  G_OBJECT_CLASS (valent_extension_parent_class)->constructed (object);

  /* If this is an extension with failable initialization, it is
   * expected to update the state with the result.
   */
  if (G_IS_INITABLE (object) || G_IS_ASYNC_INITABLE (object))
    priv->plugin_state = VALENT_PLUGIN_STATE_INACTIVE;
}
static void
valent_extension_finalize (GObject *object)
{
  ValentExtension *self = VALENT_EXTENSION (object);
  ValentExtensionPrivate *priv = valent_extension_get_instance_private (self);

  g_clear_object (&priv->plugin_info);
  g_clear_pointer (&priv->plugin_domain, g_free);
  g_clear_error (&priv->plugin_error);
  g_clear_pointer (&priv->actions, g_hash_table_unref);
  g_clear_object (&priv->settings);

  G_OBJECT_CLASS (valent_extension_parent_class)->finalize (object);
}

static void
valent_extension_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  ValentExtension *self = VALENT_EXTENSION (object);
  ValentExtensionPrivate *priv = valent_extension_get_instance_private (self);

  switch ((ValentExtensionProperty)prop_id)
    {
    case PROP_PLUGIN_DOMAIN:
      g_value_set_string (value, priv->plugin_domain);
      break;

    case PROP_PLUGIN_INFO:
      g_value_set_object (value, priv->plugin_info);
      break;

    case PROP_PLUGIN_STATE:
      g_value_set_enum (value, priv->plugin_state);
      break;

    case PROP_SETTINGS:
      g_value_set_object (value, valent_extension_get_settings (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_extension_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  ValentExtension *self = VALENT_EXTENSION (object);
  ValentExtensionPrivate *priv = valent_extension_get_instance_private (self);

  switch ((ValentExtensionProperty)prop_id)
    {
    case PROP_PLUGIN_DOMAIN:
      priv->plugin_domain = g_value_dup_string (value);
      break;

    case PROP_PLUGIN_INFO:
      priv->plugin_info = g_value_dup_object (value);
      break;

    case PROP_PLUGIN_STATE:
    case PROP_SETTINGS:
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_extension_class_init (ValentExtensionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentObjectClass *vobject_class = VALENT_OBJECT_CLASS (klass);

  object_class->constructed = valent_extension_constructed;
  object_class->finalize = valent_extension_finalize;
  object_class->get_property = valent_extension_get_property;
  object_class->set_property = valent_extension_set_property;

  vobject_class->destroy = valent_extension_destroy;

  /**
   * ValentExtension:plugin-info:
   *
   * The domain this plugin extension operates in.
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
   * ValentExtension:plugin-info:
   *
   * The [class@Peas.PluginInfo] describing this plugin.
   *
   * Since: 1.0
   */
  properties [PROP_PLUGIN_INFO] =
    g_param_spec_object ("plugin-info", NULL, NULL,
                         PEAS_TYPE_PLUGIN_INFO,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentExtension:plugin-state:
   *
   * The [enum@Valent.PluginState] describing the state of the extension.
   *
   * Since: 1.0
   */
  properties [PROP_PLUGIN_STATE] =
    g_param_spec_enum ("plugin-state", NULL, NULL,
                       VALENT_TYPE_PLUGIN_STATE,
                       VALENT_PLUGIN_STATE_ACTIVE,
                       (G_PARAM_READABLE |
                        G_PARAM_EXPLICIT_NOTIFY |
                        G_PARAM_STATIC_STRINGS));

  /**
   * ValentExtension:settings: (getter get_settings)
   *
   * The [class@Gio.Settings] for this plugin.
   *
   * Since: 1.0
   */
  properties [PROP_SETTINGS] =
    g_param_spec_object ("settings", NULL, NULL,
                         G_TYPE_SETTINGS,
                         (G_PARAM_READABLE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, G_N_ELEMENTS (properties), properties);
}

static void
valent_extension_init (ValentExtension *self)
{
  ValentExtensionPrivate *priv = valent_extension_get_instance_private (self);

  priv->actions = g_hash_table_new_full (g_str_hash,
                                         g_str_equal,
                                         g_free,
                                         g_object_unref);
}

/**
 * valent_extension_get_settings: (get-property settings)
 * @extension: a `ValentExtension`
 *
 * Get the settings for this plugin.
 *
 * Returns: (transfer none) (nullable): a `GSettings`
 *
 * Since: 1.0
 */
GSettings *
valent_extension_get_settings (ValentExtension *extension)
{
  ValentExtensionPrivate *priv = valent_extension_get_instance_private (extension);

  g_return_val_if_fail (VALENT_IS_EXTENSION (extension), NULL);

  if (priv->settings == NULL)
    {
      ValentResource *source = NULL;
      GType type_base = g_type_parent (G_OBJECT_TYPE (extension));
      const char *type_name = g_type_name (type_base);
      g_autofree char *key = NULL;

      if (g_str_has_prefix (type_name, "Valent"))
        key = g_strdup_printf ("X-%sSettings", &type_name[strlen ("Valent")]);
      else
        key = g_strdup_printf ("X-%sSettings", type_name);

      source = valent_resource_get_source (VALENT_RESOURCE (extension));
      priv->settings = valent_data_source_get_plugin_settings (VALENT_DATA_SOURCE (source),
                                                               priv->plugin_info,
                                                               key,
                                                               priv->plugin_domain);
    }

  return priv->settings;
}

/**
 * valent_extension_plugin_state_check:
 * @extension: a `ValentExtension`
 * @error: (nullable): a `GError`
 *
 * Get the extension state, while propagating any errors that describe it.
 *
 * Returns: a `ValentPluginState`
 *
 * Since: 1.0
 */
ValentPluginState
valent_extension_plugin_state_check (ValentExtension  *extension,
                                     GError          **error)
{
  ValentExtensionPrivate *priv = valent_extension_get_instance_private (extension);

  g_return_val_if_fail (VALENT_IS_EXTENSION (extension), VALENT_PLUGIN_STATE_INACTIVE);
  g_return_val_if_fail (error == NULL || *error == NULL, VALENT_PLUGIN_STATE_INACTIVE);

  if (priv->plugin_error != NULL && error != NULL)
    *error = g_error_copy (priv->plugin_error);

  return priv->plugin_state;
}

/**
 * valent_extension_plugin_state_changed:
 * @extension: a `ValentExtension`
 * @state: a `ValentPluginState`
 * @error: (nullable): a `GError`
 *
 * Emits [signal@GObject.Object::notify] for
 * [property@Valent.Extension:plugin-state].
 *
 * Implementations should call this method to inform the managing object of
 * changes to the state of the extension, especially unrecoverable errors.
 *
 * Since: 1.0
 */
void
valent_extension_plugin_state_changed (ValentExtension   *extension,
                                       ValentPluginState  state,
                                       const GError      *error)
{
  ValentExtensionPrivate *priv = valent_extension_get_instance_private (extension);

  g_return_if_fail (VALENT_IS_EXTENSION (extension));
  g_return_if_fail (state != VALENT_PLUGIN_STATE_ERROR || error != NULL);

  g_clear_error (&priv->plugin_error);
  if (state == VALENT_PLUGIN_STATE_ERROR && error != NULL)
    priv->plugin_error = g_error_copy (error);

  if (priv->plugin_state != state || priv->plugin_error != NULL)
    {
      priv->plugin_state = state;
      valent_object_notify_by_pspec (VALENT_OBJECT (extension),
                                     properties [PROP_PLUGIN_STATE]);
    }
}

/**
 * valent_extension_toggle_actions:
 * @extension: a `ValentExtension`
 * @enabled: boolean
 *
 * Enable or disable all actions.
 *
 * Set the [property@Gio.Action:enabled] property of the actions for @extension to
 * @enabled.
 *
 * Since: 1.0
 */
void
valent_extension_toggle_actions (ValentExtension *extension,
                                 gboolean         enabled)
{
  ValentExtensionPrivate *priv = valent_extension_get_instance_private (extension);
  GHashTableIter iter;
  GSimpleAction *action;

  g_return_if_fail (VALENT_IS_EXTENSION (extension));

  g_hash_table_iter_init (&iter, priv->actions);

  while (g_hash_table_iter_next (&iter, NULL, (void **)&action))
    g_simple_action_set_enabled (action, enabled);
}

