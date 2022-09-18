// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-device-plugin"

#include "config.h"

#include <gio/gio.h>
#include <json-glib/json-glib.h>
#include <libpeas/peas.h>

#include "valent-debug.h"
#include "valent-device.h"
#include "valent-device-plugin.h"
#include "valent-packet.h"

#define PLUGIN_SETTINGS_KEY "X-DevicePluginSettings"
#define PLUGIN_INCOMING_KEY "X-DevicePluginIncoming"
#define PLUGIN_OUTGOING_KEY "X-DevicePluginOutgoing"


/**
 * ValentDevicePlugin:
 *
 * An abstract base class for device plugins.
 *
 * #ValentDevicePlugin is a base class for plugins that operate in the scope of
 * a single device. This usually means communicating with other devices, however
 * plugins aren't required to be packet based and may offer connectionless
 * functionality.
 *
 * ## Plugin Requirements
 *
 * Device plugins essentially have two sets of dependent conditions for being
 * enabled. Plugins become available (i.e. can be enabled) when any of the
 * following are true:
 *
 * - any of the device's outgoing capabilities match any of the plugin's
 *   incoming capabilities
 * - any of the device's incoming capabilities match any of the plugin's
 *   outgoing capabilities
 * - the plugin doesn't list any capabilities (eg. a non-packet based plugin)
 *
 * When a plugin becomes available it may be enabled, disabled and configured.
 *
 * ## Plugin Actions
 *
 * #ValentDevicePlugin implements the [iface@Gio.ActionGroup] and
 * [iface@Gio.ActionMap] interfaces, providing a simple way for plugins to
 * expose functions and states. Each [iface@Gio.Action] added to the action map
 * will be included in the device action group with the plugin's module name as
 * a prefix (eg. `share.uri`).
 *
 * If the [class@Valent.DeviceManager] is exported on D-Bus, the actions will be
 * exported along with the [class@Valent.Device].
 *
 * ## Implementation Notes
 *
 * Implementations that define `X-DevicePluginIncoming` in the `.plugin` file
 * must override [vfunc@Valent.DevicePlugin.handle_packet] to handle incoming
 * packets. Implementations that depend on the device state, especially those
 * that define `X-DevicePluginOutgoing` in the `.plugin` file, should override
 * [vfunc@Valent.DevicePlugin.update_state].
 *
 * For device plugin preferences see [iface@Valent.DevicePreferencesPage].
 *
 * ## `.plugin` File
 *
 * Implementations may define the following extra fields in the `.plugin` file:
 *
 * - `X-DevicePluginIncoming`
 *
 *     A list of packet types (eg. `kdeconnect.ping`) separated by semi-colons
 *     indicating the packets that the plugin can handle.
 *
 * - `X-DevicePluginOutgoing`
 *
 *     A list of packet types (eg. `kdeconnect.share.request`) separated by
 *     semi-colons indicating the packets that the plugin may send.
 *
 * - `X-DevicePluginSettings`
 *
 *     A [class@Gio.Settings] schema ID for the plugin's settings. See
 *     [func@Valent.DevicePlugin.create_settings] for more information.
 *
 * - `X-ChannelProtocol`
 *
 *     A string indicating the transport protocol the plugin requires. Current
 *     possible values include `tcp` and `bluetooth`.
 *
 * Since: 1.0
 */

typedef struct
{
  ValentDevice   *device;
  PeasPluginInfo *plugin_info;
  GHashTable     *actions;
  GSettings      *settings;
} ValentDevicePluginPrivate;

static void   g_action_group_iface_init (GActionGroupInterface *iface);
static void   g_action_map_iface_init   (GActionMapInterface   *iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (ValentDevicePlugin, valent_device_plugin, VALENT_TYPE_OBJECT,
                                  G_ADD_PRIVATE (ValentDevicePlugin)
                                  G_IMPLEMENT_INTERFACE (G_TYPE_ACTION_GROUP, g_action_group_iface_init)
                                  G_IMPLEMENT_INTERFACE (G_TYPE_ACTION_MAP, g_action_map_iface_init))

enum {
  PROP_0,
  PROP_DEVICE,
  PROP_PLUGIN_INFO,
  PROP_SETTINGS,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

/**
 * ValentDevicePluginClass:
 * @enable: the virtual function pointer for valent_device_plugin_enable()
 * @disable: the virtual function pointer for valent_device_plugin_disable()
 * @handle_packet: the virtual function pointer for valent_device_plugin_handle_packet()
 * @update_state: the virtual function pointer for valent_device_plugin_update_state()
 *
 * The virtual function table for #ValentDevicePlugin.
 */


/*
 * GActionGroup
 */
static void
valent_device_plugin_activate_action (GActionGroup *action_group,
                                      const char   *action_name,
                                      GVariant     *parameter)
{
  ValentDevicePlugin *self = VALENT_DEVICE_PLUGIN (action_group);
  ValentDevicePluginPrivate *priv = valent_device_plugin_get_instance_private (self);
  GAction *action;

  if ((action = g_hash_table_lookup (priv->actions, action_name)) != NULL)
    g_action_activate (action, parameter);
}

static void
valent_device_plugin_change_action_state (GActionGroup *action_group,
                                          const char   *action_name,
                                          GVariant     *value)
{
  ValentDevicePlugin *self = VALENT_DEVICE_PLUGIN (action_group);
  ValentDevicePluginPrivate *priv = valent_device_plugin_get_instance_private (self);
  GAction *action;

  if ((action = g_hash_table_lookup (priv->actions, action_name)) != NULL)
    g_action_change_state (action, value);
}

static char **
valent_device_plugin_list_actions (GActionGroup *action_group)
{
  ValentDevicePlugin *self = VALENT_DEVICE_PLUGIN (action_group);
  ValentDevicePluginPrivate *priv = valent_device_plugin_get_instance_private (self);
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
valent_device_plugin_query_action (GActionGroup        *action_group,
                                   const char          *action_name,
                                   gboolean            *enabled,
                                   const GVariantType **parameter_type,
                                   const GVariantType **state_type,
                                   GVariant           **state_hint,
                                   GVariant           **state)
{
  ValentDevicePlugin *self = VALENT_DEVICE_PLUGIN (action_group);
  ValentDevicePluginPrivate *priv = valent_device_plugin_get_instance_private (self);
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
  iface->activate_action = valent_device_plugin_activate_action;
  iface->change_action_state = valent_device_plugin_change_action_state;
  iface->list_actions = valent_device_plugin_list_actions;
  iface->query_action = valent_device_plugin_query_action;
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
valent_device_plugin_disconnect_action (ValentDevicePlugin *self,
                                        GAction            *action)
{
  g_signal_handlers_disconnect_by_func (action, on_action_enabled_changed, self);
  g_signal_handlers_disconnect_by_func (action, on_action_state_changed, self);
}

static GAction *
valent_device_plugin_lookup_action (GActionMap *action_map,
                                    const char *action_name)
{
  ValentDevicePlugin *self = VALENT_DEVICE_PLUGIN (action_map);
  ValentDevicePluginPrivate *priv = valent_device_plugin_get_instance_private (self);

  return g_hash_table_lookup (priv->actions, action_name);
}

static void
valent_device_plugin_add_action (GActionMap *action_map,
                                 GAction    *action)
{
  ValentDevicePlugin *self = VALENT_DEVICE_PLUGIN (action_map);
  ValentDevicePluginPrivate *priv = valent_device_plugin_get_instance_private (self);
  const char *action_name;
  GAction *replacing;

  action_name = g_action_get_name (action);

  if ((replacing = g_hash_table_lookup (priv->actions, action_name)) == action)
    return;

  if (replacing != NULL)
    {
      g_action_group_action_removed (G_ACTION_GROUP (action_map), action_name);
      valent_device_plugin_disconnect_action (self, replacing);
    }

  g_signal_connect (action,
                    "notify::enabled",
                    G_CALLBACK (on_action_enabled_changed),
                    action_map);

  if (g_action_get_state_type (action) != NULL)
    g_signal_connect (action,
                      "notify::state",
                      G_CALLBACK (on_action_state_changed),
                      action_map);

  g_hash_table_replace (priv->actions,
                        g_strdup (action_name),
                        g_object_ref (action));
  g_action_group_action_added (G_ACTION_GROUP (action_map), action_name);
}

static void
valent_device_plugin_remove_action (GActionMap *action_map,
                                    const char *action_name)
{
  ValentDevicePlugin *self = VALENT_DEVICE_PLUGIN (action_map);
  ValentDevicePluginPrivate *priv = valent_device_plugin_get_instance_private (self);
  GAction *action;

  if ((action = g_hash_table_lookup (priv->actions, action_name)) != NULL)
    {
      g_action_group_action_removed (G_ACTION_GROUP (action_map), action_name);
      valent_device_plugin_disconnect_action (self, action);
      g_hash_table_remove (priv->actions, action_name);
    }
}

static void
g_action_map_iface_init (GActionMapInterface *iface)
{
  iface->add_action = valent_device_plugin_add_action;
  iface->lookup_action = valent_device_plugin_lookup_action;
  iface->remove_action = valent_device_plugin_remove_action;
}

/* LCOV_EXCL_START */
static void
valent_device_plugin_real_disable (ValentDevicePlugin *plugin)
{
  g_assert (VALENT_IS_DEVICE_PLUGIN (plugin));
}

static void
valent_device_plugin_real_enable (ValentDevicePlugin *plugin)
{
  g_assert (VALENT_IS_DEVICE_PLUGIN (plugin));
}

static void
valent_device_plugin_real_handle_packet (ValentDevicePlugin *plugin,
                                         const char         *type,
                                         JsonNode           *packet)
{
  g_assert (VALENT_IS_DEVICE_PLUGIN (plugin));
  g_assert (type != NULL && *type != '\0');
  g_assert (VALENT_IS_PACKET (packet));

  g_critical ("%s: expected handler for \"%s\" packet",
              G_OBJECT_TYPE_NAME (plugin),
              type);
}

static void
valent_device_plugin_real_update_state (ValentDevicePlugin *plugin,
                                        ValentDeviceState   state)
{
  g_assert (VALENT_IS_DEVICE_PLUGIN (plugin));
}
/* LCOV_EXCL_STOP */

/*
 * GObject
 */
static void
valent_device_plugin_constructed (GObject *object)
{
  ValentDevicePlugin *self = VALENT_DEVICE_PLUGIN (object);
  ValentDevicePluginPrivate *priv = valent_device_plugin_get_instance_private (self);
  const char *device_id = NULL;

  device_id = valent_device_get_id (priv->device);
  priv->settings = valent_device_plugin_create_settings (priv->plugin_info,
                                                         device_id);

  G_OBJECT_CLASS (valent_device_plugin_parent_class)->constructed (object);
}

static void
valent_device_plugin_dispose (GObject *object)
{
  ValentDevicePlugin *self = VALENT_DEVICE_PLUGIN (object);
  ValentDevicePluginPrivate *priv = valent_device_plugin_get_instance_private (self);
  GHashTableIter iter;
  const char *action_name;
  GAction *action;

  g_hash_table_iter_init (&iter, priv->actions);

  while (g_hash_table_iter_next (&iter, (void **)&action_name, (void **)&action))
    {
      g_action_group_action_removed (G_ACTION_GROUP (self), action_name);
      valent_device_plugin_disconnect_action (self, action);
      g_hash_table_iter_remove (&iter);
    }

  g_clear_object (&priv->settings);

  G_OBJECT_CLASS (valent_device_plugin_parent_class)->dispose (object);
}

static void
valent_device_plugin_finalize (GObject *object)
{
  ValentDevicePlugin *self = VALENT_DEVICE_PLUGIN (object);
  ValentDevicePluginPrivate *priv = valent_device_plugin_get_instance_private (self);

  g_clear_pointer (&priv->actions, g_hash_table_unref);

  G_OBJECT_CLASS (valent_device_plugin_parent_class)->finalize (object);
}

static void
valent_device_plugin_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  ValentDevicePlugin *self = VALENT_DEVICE_PLUGIN (object);
  ValentDevicePluginPrivate *priv = valent_device_plugin_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_DEVICE:
      g_value_set_object (value, priv->device);
      break;

    case PROP_PLUGIN_INFO:
      g_value_set_boxed (value, priv->plugin_info);
      break;

    case PROP_SETTINGS:
      g_value_set_object (value, priv->settings);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_device_plugin_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  ValentDevicePlugin *self = VALENT_DEVICE_PLUGIN (object);
  ValentDevicePluginPrivate *priv = valent_device_plugin_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_DEVICE:
      priv->device = g_value_get_object (value);
      break;

    case PROP_PLUGIN_INFO:
      priv->plugin_info = g_value_get_boxed (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_device_plugin_class_init (ValentDevicePluginClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = valent_device_plugin_constructed;
  object_class->dispose = valent_device_plugin_dispose;
  object_class->finalize = valent_device_plugin_finalize;
  object_class->get_property = valent_device_plugin_get_property;
  object_class->set_property = valent_device_plugin_set_property;

  klass->disable = valent_device_plugin_real_disable;
  klass->enable = valent_device_plugin_real_enable;
  klass->handle_packet = valent_device_plugin_real_handle_packet;
  klass->update_state = valent_device_plugin_real_update_state;

  /**
   * ValentDevicePlugin:device: (getter get_device)
   *
   * The [class@Valent.Device] this plugin is bound to.
   *
   * Since: 1.0
   */
  properties [PROP_DEVICE] =
    g_param_spec_object ("device", NULL, NULL,
                         VALENT_TYPE_DEVICE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentDevicePlugin:plugin-info:
   *
   * The [struct@Peas.PluginInfo] describing this plugin.
   *
   * Since: 1.0
   */
  properties [PROP_PLUGIN_INFO] =
    g_param_spec_boxed ("plugin-info", NULL, NULL,
                        PEAS_TYPE_PLUGIN_INFO,
                        (G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  /**
   * ValentDevicePlugin:settings: (getter get_settings)
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

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
valent_device_plugin_init (ValentDevicePlugin *self)
{
  ValentDevicePluginPrivate *priv = valent_device_plugin_get_instance_private (self);

  priv->actions = g_hash_table_new_full (g_str_hash,
                                         g_str_equal,
                                         g_free,
                                         g_object_unref);
}

/**
 * valent_device_plugin_get_device: (get-property device)
 * @plugin: a #ValentDevicePlugin
 *
 * Get the device this plugin is bound to.
 *
 * Returns: (transfer none): a #ValentDevice
 *
 * Since: 1.0
 */
ValentDevice *
valent_device_plugin_get_device (ValentDevicePlugin *plugin)
{
  ValentDevicePluginPrivate *priv = valent_device_plugin_get_instance_private (plugin);

  g_return_val_if_fail (VALENT_IS_DEVICE_PLUGIN (plugin), NULL);

  return priv->device;
}

/**
 * valent_device_plugin_get_settings: (get-property settings)
 * @plugin: a #ValentDevicePlugin
 *
 * Get the settings for this plugin.
 *
 * Returns: (transfer none) (nullable): a #GSettings
 *
 * Since: 1.0
 */
GSettings *
valent_device_plugin_get_settings (ValentDevicePlugin *plugin)
{
  ValentDevicePluginPrivate *priv = valent_device_plugin_get_instance_private (plugin);

  g_return_val_if_fail (VALENT_IS_DEVICE_PLUGIN (plugin), NULL);

  return priv->settings;
}

/**
 * valent_device_plugin_queue_packet:
 * @plugin: a #ValentDevicePlugin
 * @packet: a KDE Connect packet
 *
 * Queue a KDE Connect packet to be sent to the device this plugin is bound to.
 *
 * For notification of success call [method@Valent.DevicePlugin.get_device] and
 * then [method@Valent.Device.send_packet].
 *
 * Since: 1.0
 */
void
valent_device_plugin_queue_packet (ValentDevicePlugin *plugin,
                                   JsonNode           *packet)
{
  ValentDevicePluginPrivate *priv = valent_device_plugin_get_instance_private (plugin);

  g_return_if_fail (VALENT_IS_DEVICE_PLUGIN (plugin));
  g_return_if_fail (VALENT_IS_PACKET (packet));

  valent_device_queue_packet (priv->device, packet);
}

/**
 * valent_device_plugin_show_notification:
 * @plugin: a #ValentDevicePlugin
 * @id: an id for the notification
 * @notification: a #GNotification
 *
 * A convenience for showing a local notification.
 *
 * @id will be automatically prepended with the device ID and plugin module to
 * prevent conflicting with other devices and plugins.
 *
 * Call [method@Valent.DevicePlugin.hide_notification] to make the same
 * transformation on @id and withdraw the notification.
 *
 * Since: 1.0
 */
void
valent_device_plugin_show_notification (ValentDevicePlugin *plugin,
                                        const char         *id,
                                        GNotification      *notification)
{
  ValentDevicePluginPrivate *priv = valent_device_plugin_get_instance_private (plugin);
  GApplication *application = g_application_get_default ();
  g_autofree char *notification_id = NULL;

  g_return_if_fail (VALENT_IS_DEVICE_PLUGIN (plugin));
  g_return_if_fail (id != NULL);
  g_return_if_fail (G_IS_NOTIFICATION (notification));

  if G_UNLIKELY (application == NULL)
    return;

  notification_id = g_strdup_printf ("%s::%s::%s",
                                     valent_device_get_id (priv->device),
                                     peas_plugin_info_get_module_name (priv->plugin_info),
                                     id);
  g_application_send_notification (application, notification_id, notification);
}

/**
 * valent_device_plugin_hide_notification:
 * @plugin: a #ValentDevicePlugin
 * @id: an id for the notification
 *
 * A convenience for withdrawing a notification.
 *
 * This method will withdraw a notification shown with
 * [method@Valent.DevicePlugin.show_notification].
 *
 * Since: 1.0
 */
void
valent_device_plugin_hide_notification (ValentDevicePlugin *plugin,
                                        const char         *id)
{
  ValentDevicePluginPrivate *priv = valent_device_plugin_get_instance_private (plugin);
  GApplication *application = g_application_get_default ();
  g_autofree char *notification_id = NULL;

  g_return_if_fail (VALENT_IS_DEVICE_PLUGIN (plugin));
  g_return_if_fail (id != NULL);

  if G_UNLIKELY (application == NULL)
    return;

  notification_id = g_strdup_printf ("%s::%s::%s",
                                     valent_device_get_id (priv->device),
                                     peas_plugin_info_get_module_name (priv->plugin_info),
                                     id);
  g_application_withdraw_notification (application, notification_id);
}

/**
 * valent_device_plugin_create_settings:
 * @plugin_info: a #PeasPluginInfo
 * @device_id: a #ValentDevice ID
 *
 * A convenience function for plugins to create a [class@Gio.Settings] object
 * for a device plugin.
 *
 * If @plugin_info contains the `X-DevicePluginSettings` key, it is used as the
 * schema ID, otherwise this function will return %NULL.
 *
 * If the plugin is not built-in, an attempt will be made to compile the
 * [struct@Gio.SettingsSchema] in the module directory.
 *
 * Returns: (transfer full) (nullable): the new #GSettings object
 *
 * Since: 1.0
 */
GSettings *
valent_device_plugin_create_settings (PeasPluginInfo *plugin_info,
                                      const char     *device_id)
{
  GSettingsSchemaSource *default_source;
  g_autoptr (GSettingsSchema) schema = NULL;
  const char *schema_id = NULL;
  const char *module_name = NULL;
  g_autofree char *path = NULL;

  g_return_val_if_fail (plugin_info != NULL, NULL);
  g_return_val_if_fail (device_id != NULL, NULL);

  schema_id = peas_plugin_info_get_external_data (plugin_info,
                                                  "X-DevicePluginSettings");

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

  module_name = peas_plugin_info_get_module_name (plugin_info);
  path = g_strdup_printf ("/ca/andyholmes/valent/device/%s/plugin/%s/",
                          device_id, module_name);

  return g_settings_new_full (schema, NULL, path);
}

/**
 * valent_device_plugin_get_incoming:
 * @info: a #ValentDevicePluginInfo
 *
 * Get a list of incoming KDE Connect packets that the plugin described by @info
 * can handle.
 *
 * Returns: (transfer full) (nullable): a list of packet types
 *
 * Since: 1.0
 */
GStrv
valent_device_plugin_get_incoming (PeasPluginInfo *info)
{
  const char *data;

  g_return_val_if_fail (info != NULL, NULL);

  data = peas_plugin_info_get_external_data (info, "DevicePluginIncoming");

  return (data == NULL) ? NULL : g_strsplit (data, ";", -1);
}

/**
 * valent_device_plugin_get_outgoing:
 * @info: a #PeasPluginInfo
 *
 * Get a list of outgoing KDE Connect packets that the plugin described by @info
 * may provide.
 *
 * Returns: (transfer full) (nullable): a list of packet types
 *
 * Since: 1.0
 */
GStrv
valent_device_plugin_get_outgoing (PeasPluginInfo *info)
{
  const char *data;

  g_return_val_if_fail (info != NULL, NULL);

  data = peas_plugin_info_get_external_data (info, "DevicePluginOutgoing");

  return (data == NULL) ? NULL : g_strsplit (data, ";", -1);
}

/**
 * valent_device_plugin_toggle_actions:
 * @plugin: a #ValentDevicePlugin
 * @enabled: boolean
 *
 * Enable or disable all actions.
 *
 * Set the [property@Gio.Action:enabled] property of the actions for @plugin to
 * @enabled.
 *
 * Since: 1.0
 */
void
valent_device_plugin_toggle_actions (ValentDevicePlugin *plugin,
                                     gboolean            enabled)
{
  ValentDevicePluginPrivate *priv = valent_device_plugin_get_instance_private (plugin);
  GHashTableIter iter;
  GSimpleAction *action;

  g_return_if_fail (VALENT_IS_DEVICE_PLUGIN (plugin));

  g_hash_table_iter_init (&iter, priv->actions);

  while (g_hash_table_iter_next (&iter, NULL, (void **)&action))
    g_simple_action_set_enabled (action, enabled);
}

/**
 * valent_device_plugin_add_menu_entries:
 * @plugin: a #ValentDevicePlugin
 * @entries: (array length=n_entries) (element-type Valent.MenuEntry): a pointer
 *           to the first item in an array of #ValentMenuEntry structs
 * @n_entries: the length of @entries, or -1 if @entries is %NULL-terminated
 *
 * A convenience function for creating multiple [class@Gio.MenuItem] instances
 * and adding them to the device [class@Gio.MenuModel]. Each item is
 * constructed as per one [struct@Valent.MenuEntry].
 *
 * Since: 1.0
 */
void
valent_device_plugin_add_menu_entries (ValentDevicePlugin    *plugin,
                                       const ValentMenuEntry *entries,
                                       int                    n_entries)
{
  ValentDevicePluginPrivate *priv = valent_device_plugin_get_instance_private (plugin);
  GMenu *menu;

  g_return_if_fail (VALENT_IS_DEVICE_PLUGIN (plugin));
  g_return_if_fail (entries != NULL || n_entries == 0);

  menu = G_MENU (valent_device_get_menu (priv->device));

  for (int i = 0; i < n_entries; i++)
    {
      g_autoptr (GMenuItem) item = NULL;
      g_autoptr (GIcon) icon = NULL;

      icon = g_themed_icon_new (entries[i].icon_name);
      item = g_menu_item_new(entries[i].label, entries[i].action);
      g_menu_item_set_icon (item, icon);
      g_menu_item_set_attribute (item, "hidden-when", "s", "action-disabled");
      g_menu_append_item (menu, item);
    }
}

static int
_g_menu_find_action (GMenu      *menu,
                     const char *action)
{
  GMenuModel *model = G_MENU_MODEL (menu);
  int i, n_items;

  g_return_val_if_fail (G_IS_MENU (menu), -1);
  g_return_val_if_fail (action != NULL, -1);

  n_items = g_menu_model_get_n_items (model);

  for (i = 0; i < n_items; i++)
    {
      g_autofree char *item_str = NULL;

      g_menu_model_get_item_attribute (model, i, "action", "s", &item_str);

      if (g_strcmp0 (item_str, action) == 0)
        return i;
    }

  return -1;
}

/**
 * valent_device_plugin_remove_menu_entries:
 * @plugin: a #ValentDevicePlugin
 * @entries: (array length=n_entries) (element-type Valent.MenuEntry): a pointer
 *           to the first item in an array of #ValentMenuEntry structs
 * @n_entries: the length of @entries, or -1 if @entries is %NULL-terminated
 *
 * A counterpart to [method@Valent.DevicePlugin.add_menu_entries].
 *
 * Since: 1.0
 */
void
valent_device_plugin_remove_menu_entries (ValentDevicePlugin    *plugin,
                                          const ValentMenuEntry *entries,
                                          int                    n_entries)
{
  ValentDevicePluginPrivate *priv = valent_device_plugin_get_instance_private (plugin);
  GMenu *menu;

  g_return_if_fail (VALENT_IS_DEVICE_PLUGIN (plugin));
  g_return_if_fail (entries != NULL || n_entries == 0);

  menu = G_MENU (valent_device_get_menu (priv->device));

  for (int i = 0; i < n_entries; i++)
    {
      int index = _g_menu_find_action (menu, entries[i].action);

      if (index > -1)
        g_menu_remove (menu, index);
    }
}

/**
 * valent_device_plugin_find_menu_item:
 * @plugin: a #ValentDevicePlugin
 * @attribute: an attribute name to match
 * @value: an attribute value to match
 *
 * Search the top-level of a [class@Gio.MenuModel] for the index of an item with
 * the attribute @name holding @value.
 *
 * Returns: position of the item or -1 if not found
 *
 * Since: 1.0
 */
int
valent_device_plugin_find_menu_item (ValentDevicePlugin *plugin,
                                     const char         *attribute,
                                     const GVariant     *value)
{
  ValentDevicePluginPrivate *priv = valent_device_plugin_get_instance_private (plugin);
  GMenuModel *model;
  int i, n_items;

  g_return_val_if_fail (VALENT_IS_DEVICE_PLUGIN (plugin), -1);
  g_return_val_if_fail (attribute != NULL, -1);
  g_return_val_if_fail (value != NULL, -1);

  model = valent_device_get_menu (priv->device);

  n_items = g_menu_model_get_n_items (model);

  for (i = 0; i < n_items; i++)
    {
      g_autoptr (GVariant) ivar = NULL;

      ivar = g_menu_model_get_item_attribute_value (model, i, attribute, NULL);

      if (ivar == NULL)
        continue;

      if (g_variant_equal (value, ivar))
        return i;
    }

  return -1;
}

/**
 * valent_device_plugin_remove_menu_item:
 * @plugin: a #ValentDevicePlugin
 * @attribute: an attribute name to match
 * @value: an attribute value to match
 *
 * Remove an item in the top-level of the device [class@Gio.MenuModel] with
 * the specified attribute and value.
 *
 * Returns: the index of the removed item or -1 if not found.
 *
 * Since: 1.0
 */
int
valent_device_plugin_remove_menu_item (ValentDevicePlugin *plugin,
                                       const char         *attribute,
                                       const GVariant     *value)
{
  ValentDevicePluginPrivate *priv = valent_device_plugin_get_instance_private (plugin);
  GMenu *menu;
  int position;

  g_return_val_if_fail (VALENT_IS_DEVICE_PLUGIN (plugin), -1);
  g_return_val_if_fail (attribute != NULL, -1);
  g_return_val_if_fail (value != NULL, -1);

  menu = G_MENU (valent_device_get_menu (priv->device));
  position = valent_device_plugin_find_menu_item (plugin, attribute, value);

  if (position > -1)
    g_menu_remove (menu, position);

  return position;
}

/**
 * valent_device_plugin_replace_menu_item:
 * @plugin: a #ValentDevicePlugin
 * @item: a #GMenuItem
 * @attribute: an attribute name to match
 *
 * Replace an item in the top-level of the device [class@Gio.MenuModel] with
 * @item.
 *
 * If the devices menu does not contain a top-level item with the same action
 * name as @item, the item will be appended instead.
 *
 * Since: 1.0
 */
void
valent_device_plugin_replace_menu_item (ValentDevicePlugin *plugin,
                                        GMenuItem          *item,
                                        const char         *attribute)
{
  ValentDevicePluginPrivate *priv = valent_device_plugin_get_instance_private (plugin);
  g_autoptr (GVariant) value = NULL;
  GMenu *menu;
  int position = -1;

  g_return_if_fail (VALENT_IS_DEVICE_PLUGIN (plugin));
  g_return_if_fail (G_IS_MENU_ITEM (item));

  menu = G_MENU (valent_device_get_menu (priv->device));
  value = g_menu_item_get_attribute_value (item, attribute, NULL);

  if (value != NULL)
    position = valent_device_plugin_remove_menu_item (plugin, attribute, value);

  if (position > -1)
    g_menu_insert_item (menu, position, item);
  else
    g_menu_append_item (menu, item);
}

/**
 * valent_device_plugin_enable: (virtual enable)
 * @plugin: a #ValentDevicePlugin
 *
 * Enable the plugin.
 *
 * This function is called when the plugin is enabled by the user and should
 * prepare any persistent resources it may need. Usually this means registering
 * actions, preparing the plugin settings and other data sources.
 *
 * It is guaranteed that [method@Valent.DevicePlugin.disable] will be called if
 * this function has been called.
 *
 * Since: 1.0
 */
void
valent_device_plugin_enable (ValentDevicePlugin *plugin)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_DEVICE_PLUGIN (plugin));

  VALENT_DEVICE_PLUGIN_GET_CLASS (plugin)->enable (plugin);

  VALENT_EXIT;
}

/**
 * valent_device_plugin_disable: (virtual disable)
 * @plugin: a #ValentDevicePlugin
 *
 * Disable the plugin.
 *
 * This function is called when the plugin is disabled by the user and should
 * clean up any resources prepared in [method@Valent.DevicePlugin.enable] or
 * [method@Valent.DevicePlugin.update_state].
 *
 * It is guaranteed that this function will be called if
 * [method@Valent.DevicePlugin.enable] has been called.
 *
 * Since: 1.0
 */
void
valent_device_plugin_disable (ValentDevicePlugin *plugin)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_DEVICE_PLUGIN (plugin));

  VALENT_DEVICE_PLUGIN_GET_CLASS (plugin)->disable (plugin);

  VALENT_EXIT;
}

/**
 * valent_device_plugin_handle_packet: (virtual handle_packet)
 * @plugin: a #ValentDevicePlugin
 * @type: a KDE Connect packet type
 * @packet: a KDE Connect packet
 *
 * Handle a packet from the device the plugin is bound to.
 *
 * This is called when the device receives a packet type included in the
 * `X-DevicePluginIncoming` field of the `.plugin` file.
 *
 * This is optional for implementations which do not register any incoming
 * capabilities, such as plugins that do not provide packet-based functionality.
 *
 * Since: 1.0
 */
void
valent_device_plugin_handle_packet (ValentDevicePlugin *plugin,
                                    const char         *type,
                                    JsonNode           *packet)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_DEVICE_PLUGIN (plugin));
  g_return_if_fail (type != NULL && *type != '\0');
  g_return_if_fail (VALENT_IS_PACKET (packet));

  VALENT_DEVICE_PLUGIN_GET_CLASS (plugin)->handle_packet (plugin, type, packet);

  VALENT_EXIT;
}

/**
 * valent_device_plugin_update_state: (virtual update_state)
 * @plugin: a #ValentDevicePlugin
 * @state: a #ValentDeviceState
 *
 * Update the plugin based on the new state of the device.
 *
 * This function is called when the connected or paired state of the device
 * changes. This may be used to configure actions, event handlers that may
 * trigger outgoing packets and exchange connect-time data with the device.
 *
 * This is optional for all implementations as plugins aren't required to be
 * dependent on the device state.
 *
 * Since: 1.0
 */
void
valent_device_plugin_update_state (ValentDevicePlugin *plugin,
                                   ValentDeviceState   state)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_DEVICE_PLUGIN (plugin));

  VALENT_DEVICE_PLUGIN_GET_CLASS (plugin)->update_state (plugin, state);

  VALENT_EXIT;
}


// TODO move to libvalent-notification

/**
 * valent_notification_set_device_action:
 * @notification: a #GNotification
 * @device: a #ValentDevice
 * @action: the device action name
 * @target: (nullable): the action target
 *
 * Set the default action for @notification. @action is wrapped in the special
 * `device` action for @device, which allows it to be activated from the `app`
 * action scope.
 *
 * Since: 1.0
 */
void
valent_notification_set_device_action (GNotification *notification,
                                       ValentDevice  *device,
                                       const char    *action,
                                       GVariant      *target)
{
  GVariantBuilder builder;

  g_return_if_fail (G_IS_NOTIFICATION (notification));
  g_return_if_fail (VALENT_IS_DEVICE (device));
  g_return_if_fail (action != NULL && *action != '\0');

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("av"));

  if (target != NULL)
    g_variant_builder_add (&builder, "v", target);

  g_notification_set_default_action_and_target (notification,
                                                "app.device",
                                                "(ssav)",
                                                valent_device_get_id (device),
                                                action,
                                                &builder);
}

/**
 * valent_notification_add_device_button:
 * @notification: a #GNotification
 * @device: a #ValentDevice
 * @label: the button label
 * @action: the device action name
 * @target: (nullable): the action target
 *
 * Add an action button to @notification. @action is wrapped in the special
 * `device` action for @device, which allows it to be activated from the `app`
 * action scope.
 *
 * Since: 1.0
 */
void
valent_notification_add_device_button (GNotification *notification,
                                       ValentDevice  *device,
                                       const char    *label,
                                       const char    *action,
                                       GVariant      *target)
{
  GVariantBuilder builder;

  g_return_if_fail (G_IS_NOTIFICATION (notification));
  g_return_if_fail (VALENT_IS_DEVICE (device));
  g_return_if_fail (label != NULL && *label != '\0');
  g_return_if_fail (action != NULL && *action != '\0');

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("av"));

  if (target != NULL)
    g_variant_builder_add (&builder, "v", target);

  g_notification_add_button_with_target (notification,
                                         label,
                                         "app.device",
                                         "(ssav)",
                                         valent_device_get_id (device),
                                         action,
                                         &builder);
}

