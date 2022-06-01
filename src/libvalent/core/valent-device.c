// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-device"

#include "config.h"

#include <glib/gi18n.h>
#include <gio/gio.h>

#include "valent-core-enums.h"

#include "valent-channel.h"
#include "valent-data.h"
#include "valent-debug.h"
#include "valent-device.h"
#include "valent-device-plugin.h"
#include "valent-device-private.h"
#include "valent-macros.h"
#include "valent-object.h"
#include "valent-packet.h"
#include "valent-utils.h"

#define PAIR_REQUEST_ID      "pair-request"
#define PAIR_REQUEST_TIMEOUT 30


/**
 * ValentDevice:
 *
 * A class representing a remote device, such as a smartphone or desktop.
 *
 * Device functionality is limited to pairing and sending packets, while other
 * functionality is delegated to [class@Valent.DevicePlugin] extensions.
 *
 * #ValentDevice implements the [iface@Gio.ActionGroup] interface, acting as an
 * aggregate action group for plugins. Plugin actions are automatically included
 * in the device action group with the plugin module name as a prefix
 * (eg. `share.files`).
 *
 * Since: 1.0
 */

struct _ValentDevice
{
  ValentObject    parent_instance;

  ValentData     *data;
  GSettings      *settings;

  /* Properties */
  char           *icon_name;
  char           *id;
  char           *name;
  char           *type;
  char          **incoming_capabilities;
  char          **outgoing_capabilities;

  /* State */
  ValentChannel  *channel;
  gboolean        paired;
  unsigned int    incoming_pair;
  unsigned int    outgoing_pair;

  /* Plugins */
  PeasEngine     *engine;
  GHashTable     *plugins;
  GHashTable     *handlers;
  GHashTable     *actions;
  GMenu          *menu;
};

static void       valent_device_reload_plugins  (ValentDevice   *device);
static void       valent_device_update_plugins  (ValentDevice   *device);
static gboolean   valent_device_supports_plugin (ValentDevice   *device,
                                                 PeasPluginInfo *info);

static void   g_action_group_iface_init     (GActionGroupInterface *iface);

G_DEFINE_TYPE_WITH_CODE (ValentDevice, valent_device, VALENT_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_ACTION_GROUP, g_action_group_iface_init))


enum {
  PROP_0,
  PROP_CONNECTED,
  PROP_DATA,
  PROP_ICON_NAME,
  PROP_ID,
  PROP_NAME,
  PROP_PAIRED,
  PROP_STATE,
  PROP_TYPE,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

enum {
  PLUGIN_ADDED,
  PLUGIN_REMOVED,
  N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0, };


/*
 * GActionGroup
 */
static void
valent_device_activate_action (GActionGroup *action_group,
                               const char   *action_name,
                               GVariant     *parameter)
{
  ValentDevice *self = VALENT_DEVICE (action_group);
  GAction *action;

  if ((action = g_hash_table_lookup (self->actions, action_name)) != NULL)
    g_action_activate (action, parameter);
}

static void
valent_device_change_action_state (GActionGroup *action_group,
                                   const char   *action_name,
                                   GVariant     *value)
{
  ValentDevice *self = VALENT_DEVICE (action_group);
  GAction *action;

  if ((action = g_hash_table_lookup (self->actions, action_name)) != NULL)
    g_action_change_state (action, value);
}

static char **
valent_device_list_actions (GActionGroup *action_group)
{
  ValentDevice *self = VALENT_DEVICE (action_group);
  g_auto (GStrv) actions = NULL;
  GHashTableIter iter;
  gpointer key;
  unsigned int i = 0;

  actions = g_new0 (char *, g_hash_table_size (self->actions) + 1);

  g_hash_table_iter_init (&iter, self->actions);

  while (g_hash_table_iter_next (&iter, &key, NULL))
    actions[i++] = g_strdup (key);

  return g_steal_pointer (&actions);
}

static gboolean
valent_device_query_action (GActionGroup        *action_group,
                            const char          *action_name,
                            gboolean            *enabled,
                            const GVariantType **parameter_type,
                            const GVariantType **state_type,
                            GVariant           **state_hint,
                            GVariant           **state)
{
  ValentDevice *self = VALENT_DEVICE (action_group);
  GAction *action;

  if ((action = g_hash_table_lookup (self->actions, action_name)) == NULL)
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
  iface->activate_action = valent_device_activate_action;
  iface->change_action_state = valent_device_change_action_state;
  iface->list_actions = valent_device_list_actions;
  iface->query_action = valent_device_query_action;
}


/*
 * Private plugin methods
 */
typedef struct
{
  ValentDevice   *device;
  PeasPluginInfo *info;
  PeasExtension  *extension;
  GSettings      *settings;
} DevicePlugin;

static void
device_plugin_free (gpointer data)
{
  DevicePlugin *plugin = data;

  /* We guarantee calling valent_device_plugin_disable() */
  if (plugin->extension != NULL)
    {
      valent_device_plugin_disable (VALENT_DEVICE_PLUGIN (plugin->extension));
      g_clear_object (&plugin->extension);
    }

  g_clear_object (&plugin->settings);
  g_clear_pointer (&plugin, g_free);
}

static void
on_plugin_action_added (GActionGroup *action_group,
                        const char   *action_name,
                        DevicePlugin *plugin)
{
  ValentDevice *self = VALENT_DEVICE (plugin->device);
  g_autofree char *full_name = NULL;
  GAction *action;

  full_name = g_strdup_printf ("%s.%s",
                               peas_plugin_info_get_module_name (plugin->info),
                               action_name);
  action = g_action_map_lookup_action (G_ACTION_MAP (action_group),
                                       action_name);

  g_hash_table_replace (self->actions,
                        g_strdup (full_name),
                        g_object_ref (action));
  g_action_group_action_added (G_ACTION_GROUP (plugin->device), full_name);
}

static void
on_plugin_action_enabled_changed (GActionGroup *action_group,
                                  const char   *action_name,
                                  gboolean      enabled,
                                  DevicePlugin *plugin)
{
  g_autofree char *full_name = NULL;

  full_name = g_strdup_printf ("%s.%s",
                               peas_plugin_info_get_module_name (plugin->info),
                               action_name);
  g_action_group_action_enabled_changed (G_ACTION_GROUP (plugin->device),
                                         full_name,
                                         enabled);
}

static void
on_plugin_action_removed (GActionGroup *action_group,
                          const char   *action_name,
                          DevicePlugin *plugin)
{
  ValentDevice *self = VALENT_DEVICE (plugin->device);
  g_autofree char *full_name = NULL;

  full_name = g_strdup_printf ("%s.%s",
                               peas_plugin_info_get_module_name (plugin->info),
                               action_name);

  g_action_group_action_removed (G_ACTION_GROUP (plugin->device), full_name);
  g_hash_table_remove (self->actions, full_name);
}

static void
on_plugin_action_state_changed (GActionGroup *action_group,
                                const char   *action_name,
                                GVariant     *value,
                                DevicePlugin *plugin)
{
  g_autofree char *full_name = NULL;

  full_name = g_strdup_printf ("%s.%s",
                               peas_plugin_info_get_module_name (plugin->info),
                               action_name);
  g_action_group_action_state_changed (G_ACTION_GROUP (plugin->device),
                                       full_name,
                                       value);
}

static void
valent_device_enable_plugin (ValentDevice *device,
                             DevicePlugin *plugin)
{
  g_auto (GStrv) actions = NULL;
  g_auto (GStrv) incoming = NULL;
  unsigned int n_capabilities = 0;

  g_assert (VALENT_IS_DEVICE (device));
  g_assert (plugin != NULL);

  /* Instantiate the plugin */
  plugin->extension = peas_engine_create_extension (device->engine,
                                                    plugin->info,
                                                    VALENT_TYPE_DEVICE_PLUGIN,
                                                    "device", device,
                                                    NULL);
  g_return_if_fail (PEAS_IS_EXTENSION (plugin->extension));

  /* Register packet handlers */
  if ((incoming = valent_device_plugin_get_incoming (plugin->info)) != NULL)
    n_capabilities = g_strv_length (incoming);

  for (unsigned int i = 0; i < n_capabilities; i++)
    {
      g_hash_table_insert (device->handlers,
                           g_strdup (incoming[i]),
                           plugin->extension);
    }

  /* Register plugin actions */
  actions = g_action_group_list_actions (G_ACTION_GROUP (plugin->extension));

  for (unsigned int i = 0; actions[i] != NULL; i++)
    {
      on_plugin_action_added (G_ACTION_GROUP (plugin->extension),
                              actions[i],
                              plugin);
    }

  g_signal_connect (plugin->extension,
                    "action-added",
                    G_CALLBACK (on_plugin_action_added),
                    plugin);
  g_signal_connect (plugin->extension,
                    "action-enabled-changed",
                    G_CALLBACK (on_plugin_action_enabled_changed),
                    plugin);
  g_signal_connect (plugin->extension,
                    "action-removed",
                    G_CALLBACK (on_plugin_action_removed),
                    plugin);
  g_signal_connect (plugin->extension,
                    "action-state-changed",
                    G_CALLBACK (on_plugin_action_state_changed),
                    plugin);

  /* Bootstrap the newly instantiated plugin */
  valent_device_plugin_enable (VALENT_DEVICE_PLUGIN (plugin->extension));
  valent_device_plugin_update_state (VALENT_DEVICE_PLUGIN (plugin->extension),
                                     valent_device_get_state (device));
}

static void
valent_device_disable_plugin (ValentDevice *device,
                              DevicePlugin *plugin)
{
  g_auto (GStrv) actions = NULL;
  g_auto (GStrv) incoming = NULL;
  unsigned int len;

  g_assert (VALENT_IS_DEVICE (device));
  g_assert (plugin != NULL);
  g_return_if_fail (PEAS_IS_EXTENSION (plugin->extension));

  /* Unregister actions */
  g_signal_handlers_disconnect_by_data (plugin->extension, plugin);
  actions = g_action_group_list_actions (G_ACTION_GROUP (plugin->extension));

  for (unsigned int i = 0; actions[i]; i++)
    {
      on_plugin_action_removed (G_ACTION_GROUP (plugin->extension),
                                actions[i],
                                plugin);
    }

  /* Unregister packet handlers */
  incoming = valent_device_plugin_get_incoming (plugin->info);
  len = incoming ? g_strv_length (incoming) : 0;

  for (unsigned int i = 0; i < len; i++)
    g_hash_table_remove (device->handlers, incoming[i]);

  /* Invoke the plugin vfunc */
  valent_device_plugin_disable (VALENT_DEVICE_PLUGIN (plugin->extension));
  g_clear_object (&plugin->extension);
}

static void
on_enabled_changed (GSettings    *settings,
                    const char   *key,
                    DevicePlugin *plugin)
{
  g_assert (G_IS_SETTINGS (settings));
  g_assert (key != NULL);
  g_assert (plugin != NULL);

  if (g_settings_get_boolean (settings, "enabled"))
    valent_device_enable_plugin (plugin->device, plugin);
  else
    valent_device_disable_plugin (plugin->device, plugin);
}


/*
 * Private pairing methods
 */
static gboolean
valent_device_reset_pair (gpointer object)
{
  ValentDevice *device = VALENT_DEVICE (object);
  GApplication *application = g_application_get_default ();

  g_assert (VALENT_IS_DEVICE (device));

  if (application != NULL)
    {
      g_autofree char *notification_id = NULL;

      notification_id = g_strdup_printf ("%s::%s", device->id, PAIR_REQUEST_ID);
      g_application_withdraw_notification (application, notification_id);
    }

  g_clear_handle_id (&device->incoming_pair, g_source_remove);
  g_clear_handle_id (&device->outgoing_pair, g_source_remove);

  valent_object_notify_by_pspec (G_OBJECT (device), properties [PROP_STATE]);

  return G_SOURCE_REMOVE;
}

static void
send_pair_cb (ValentChannel *channel,
              GAsyncResult  *result,
              ValentDevice  *device)
{
  g_autoptr (GError) error = NULL;

  g_assert (VALENT_IS_CHANNEL (channel));
  g_assert (VALENT_IS_DEVICE (device));

  if (!valent_channel_write_packet_finish (channel, result, &error))
    {
      VALENT_NOTE ("%s: %s", device->name, error->message);

      valent_device_reset_pair (device);

      valent_object_lock (VALENT_OBJECT (device));
      if (device->channel == channel)
        valent_device_set_channel (device, NULL);
      valent_object_unlock (VALENT_OBJECT (device));
    }

  g_object_unref (device);
}

static void
valent_device_send_pair (ValentDevice *device,
                         gboolean      pair)
{
  JsonBuilder *builder;
  g_autoptr (JsonNode) packet = NULL;
  g_autoptr (GCancellable) cancellable = NULL;

  g_assert (VALENT_IS_DEVICE (device));

  valent_object_lock (VALENT_OBJECT (device));

  if (device->channel == NULL)
    {
      valent_object_unlock (VALENT_OBJECT (device));
      return;
    }

  builder = valent_packet_start ("kdeconnect.pair");
  json_builder_set_member_name (builder, "pair");
  json_builder_add_boolean_value (builder, pair);
  packet = valent_packet_finish (builder);

  valent_channel_write_packet (device->channel,
                               packet,
                               cancellable,
                               (GAsyncReadyCallback)send_pair_cb,
                               g_object_ref (device));

  valent_object_unlock (VALENT_OBJECT (device));
}

static void
valent_device_notify_pair (ValentDevice *device)
{
  GApplication *application = g_application_get_default ();

  g_assert (VALENT_IS_DEVICE (device));

  if (application != NULL)
    {
      g_autofree char *notification_id = NULL;
      g_autoptr (GNotification) notification = NULL;
      g_autoptr (GIcon) icon = NULL;
      g_autofree char *title = NULL;
      const char *body;

      title = g_strdup_printf (_("Pairing request from %s"), device->name);
      notification = g_notification_new (title);

      if ((body = valent_channel_get_verification_key (device->channel)) != NULL)
        g_notification_set_body (notification, body);

      icon = g_themed_icon_new (APPLICATION_ID);
      g_notification_set_icon (notification, icon);

      g_notification_set_priority (notification, G_NOTIFICATION_PRIORITY_URGENT);

      g_notification_add_button_with_target (notification, _("Reject"), "app.device",
                                             "(ssav)",
                                             device->id,
                                             "unpair",
                                             NULL);

      g_notification_add_button_with_target (notification, _("Accept"), "app.device",
                                             "(ssav)",
                                             device->id,
                                             "pair",
                                             NULL);

      /* Show the pairing notification and set a timeout for 30s */
      notification_id = g_strdup_printf ("%s::%s", device->id, PAIR_REQUEST_ID);
      g_application_send_notification (application,
                                       notification_id,
                                       notification);
    }

  device->incoming_pair = g_timeout_add_seconds (PAIR_REQUEST_TIMEOUT,
                                                 valent_device_reset_pair,
                                                 device);
  valent_object_notify_by_pspec (G_OBJECT (device), properties [PROP_STATE]);
}

static void
valent_device_handle_pair (ValentDevice *device,
                           JsonNode     *packet)
{
  gboolean pair;

  VALENT_ENTRY;

  g_assert (VALENT_IS_DEVICE (device));
  g_assert (VALENT_IS_PACKET (packet));

  if (!valent_packet_get_boolean (packet, "pair", &pair))
    {
      g_warning ("%s(): malformed pair packet from \"%s\"",
                 G_STRFUNC,
                 device->name);
      VALENT_EXIT;
    }

  /* Device is requesting pairing or accepting our request */
  if (pair)
    {
      /* The device is accepting our request */
      if (device->outgoing_pair > 0)
        {
          VALENT_NOTE ("Pairing accepted by \"%s\"", device->name);
          valent_device_set_paired (device, TRUE);
        }

      /* The device thinks we're unpaired */
      else if (device->paired)
        {
          valent_device_send_pair (device, TRUE);
          valent_device_set_paired (device, TRUE);
        }

      /* The device is requesting pairing */
      else
        {
          VALENT_NOTE ("Pairing requested by \"%s\"", device->name);
          valent_device_notify_pair (device);
        }
    }

  /* Device is requesting unpairing or rejecting our request */
  else
    {
      VALENT_NOTE ("Pairing rejected by \"%s\"", device->name);
      valent_device_set_paired (device, FALSE);
    }

  VALENT_EXIT;
}

/*
 * Private identity methods
 */
static void
valent_device_handle_identity (ValentDevice *device,
                               JsonNode     *packet)
{
  const char *device_id;
  const char *device_name;
  const char *device_type;

  VALENT_ENTRY;

  g_assert (VALENT_IS_DEVICE (device));
  g_assert (VALENT_IS_PACKET (packet));

  valent_object_lock (VALENT_OBJECT (device));

  /* Device ID, which MUST exist and MUST match the construct-time value */
  if (!valent_packet_get_string (packet, "deviceId", &device_id) ||
      !g_str_equal (device->id, device_id))
    {
      g_critical ("%s(): expected \"deviceId\" field holding \"%s\"",
                  G_STRFUNC,
                  device->id);
      valent_object_unlock (VALENT_OBJECT (device));
      VALENT_EXIT;
    }

  /* Device Name */
  if (!valent_packet_get_string (packet, "deviceName", &device_name))
    device_name = "Unnamed";

  if (g_strcmp0 (device->name, device_name) != 0)
    {
      g_clear_pointer (&device->name, g_free);
      device->name = g_strdup (device_name);
      g_object_notify_by_pspec (G_OBJECT (device), properties [PROP_NAME]);
    }

  /* Device Type */
  if (!valent_packet_get_string (packet, "deviceType", &device_type))
    device_type = "desktop";

  if (g_strcmp0 (device->type, device_type) != 0)
    {
      const char *device_icon = "computer-symbolic";

      if (g_str_equal (device_type, "desktop"))
        device_icon = "computer-symbolic";
      else if (g_str_equal (device_type, "laptop"))
        device_icon = "laptop-symbolic";
      else if (g_str_equal (device_type, "phone"))
        device_icon = "phone-symbolic";
      else if (g_str_equal (device_type, "tablet"))
        device_icon = "tablet-symbolic";
      else if (g_str_equal (device_type, "tv"))
        device_icon = "tv-symbolic";

      g_clear_pointer (&device->icon_name, g_free);
      device->icon_name = g_strdup (device_icon);
      g_object_notify_by_pspec (G_OBJECT (device), properties [PROP_ICON_NAME]);

      g_clear_pointer (&device->type, g_free);
      device->type = g_strdup (device_type);
      g_object_notify_by_pspec (G_OBJECT (device), properties [PROP_TYPE]);
    }

  /* Generally, these should be static, but could change if the connection type
   * changes between eg. TCP and Bluetooth */
  g_clear_pointer (&device->incoming_capabilities, g_strfreev);
  device->incoming_capabilities = valent_packet_dup_strv (packet,
                                                          "incomingCapabilities");

  g_clear_pointer (&device->outgoing_capabilities, g_strfreev);
  device->outgoing_capabilities = valent_packet_dup_strv (packet,
                                                          "outgoingCapabilities");

  valent_object_unlock (VALENT_OBJECT (device));

  /* Recheck plugins and load or unload if capabilities have changed */
  valent_device_reload_plugins (device);

  VALENT_EXIT;
}


/*
 * ValentEngine callbacks
 */
static void
on_load_plugin (PeasEngine     *engine,
                PeasPluginInfo *info,
                ValentDevice   *device)
{
  DevicePlugin *plugin;
  const char *module;
  g_autofree char *path = NULL;

  g_assert (PEAS_IS_ENGINE (engine));
  g_assert (info != NULL);
  g_assert (VALENT_IS_DEVICE (device));

  if (!valent_device_supports_plugin (device, info))
    return;

  if (g_hash_table_contains (device->plugins, info))
    return;

  VALENT_NOTE ("%s: %s",
               device->name,
               peas_plugin_info_get_module_name (info));

  /* Register the plugin & data (hash tables are ref owners) */
  module = peas_plugin_info_get_module_name (info);
  path = g_strdup_printf ("/ca/andyholmes/valent/device/%s/plugin/%s/",
                          device->id, module);

  plugin = g_new0 (DevicePlugin, 1);
  plugin->device = device;
  plugin->info = info;
  plugin->settings = g_settings_new_with_path ("ca.andyholmes.Valent.Plugin",
                                               path);
  g_hash_table_insert (device->plugins, info, plugin);

  /* The PeasExtension is created and destroyed based on the enabled state */
  g_signal_connect (plugin->settings,
                    "changed::enabled",
                    G_CALLBACK (on_enabled_changed),
                    plugin);

  if (g_settings_get_boolean (plugin->settings, "enabled"))
    valent_device_enable_plugin (device, plugin);

  /* Notify now so that plugins can be configured regardless of device state */
  g_signal_emit (G_OBJECT (device), signals [PLUGIN_ADDED], 0, info);
}

static void
on_unload_plugin (PeasEngine     *engine,
                  PeasPluginInfo *info,
                  ValentDevice   *device)
{
  g_assert (PEAS_IS_ENGINE (engine));
  g_assert (info != NULL);
  g_assert (VALENT_IS_DEVICE (device));

  if (!g_hash_table_contains (device->plugins, info))
    return;

  VALENT_NOTE ("%s: %s",
               device->name,
               peas_plugin_info_get_module_name (info));

  g_hash_table_remove (device->plugins, info);
  g_signal_emit (G_OBJECT (device), signals [PLUGIN_REMOVED], 0, info);
}


/*
 * GActions
 */
static void
pair_action (GSimpleAction *action,
             GVariant      *parameter,
             gpointer       user_data)
{
  ValentDevice *device = VALENT_DEVICE (user_data);

  /* We're accepting an incoming pair request */
  if (device->incoming_pair > 0)
    {
      valent_device_send_pair (device, TRUE);
      valent_device_set_paired (device, TRUE);
    }

  /* We're initiating an outgoing pair request */
  else if (!device->paired)
    {
      valent_device_reset_pair (device);
      valent_device_send_pair (device, TRUE);
      device->outgoing_pair = g_timeout_add_seconds (PAIR_REQUEST_TIMEOUT,
                                                     valent_device_reset_pair,
                                                     device);
      VALENT_NOTE ("Pair request sent to \"%s\"", device->name);

      valent_object_notify_by_pspec (G_OBJECT (device), properties [PROP_STATE]);
    }
}

static void
unpair_action (GSimpleAction *action,
               GVariant      *parameter,
               gpointer       user_data)
{
  ValentDevice *device = VALENT_DEVICE (user_data);

  valent_device_send_pair (device, FALSE);
  valent_device_set_paired (device, FALSE);
}

/*
 * GObject
 */
static void
valent_device_constructed (GObject *object)
{
  ValentDevice *self = VALENT_DEVICE (object);
  g_autofree char *path = NULL;
  g_autofree char *download_folder = NULL;
  const GList *plugins = NULL;

  /* We must at least have a device ID */
  g_assert (self->id != NULL);

  /* Data Manager */
  if (self->data == NULL)
    self->data = valent_data_new (self->id, NULL);

  /* GSettings*/
  path = g_strdup_printf ("/ca/andyholmes/valent/device/%s/", self->id);
  self->settings = g_settings_new_with_path ("ca.andyholmes.Valent.Device", path);
  self->paired = g_settings_get_boolean (self->settings, "paired");

  download_folder = g_settings_get_string (self->settings, "download-folder");

  if (strlen (download_folder) == 0)
    {
      g_clear_pointer (&download_folder, g_free);
      download_folder = valent_data_get_directory (G_USER_DIRECTORY_DOWNLOAD);
      g_settings_set_string (self->settings, "download-folder", download_folder);
    }

  /* Load plugins and watch for changes */
  plugins = peas_engine_get_plugin_list (self->engine);

  for (const GList *iter = plugins; iter; iter = iter->next)
    {
      if (peas_plugin_info_is_loaded (iter->data))
        on_load_plugin (self->engine, iter->data, self);
    }

  g_signal_connect_object (self->engine,
                           "load-plugin",
                           G_CALLBACK (on_load_plugin),
                           self,
                           G_CONNECT_AFTER);

  g_signal_connect_object (self->engine,
                           "unload-plugin",
                           G_CALLBACK (on_unload_plugin),
                           self,
                           0);

  G_OBJECT_CLASS (valent_device_parent_class)->constructed (object);
}

static void
valent_device_dispose (GObject *object)
{
  ValentDevice *self = VALENT_DEVICE (object);

  /* State */
  valent_device_reset_pair (self);
  valent_device_set_channel (self, NULL);

  /* Plugins */
  g_signal_handlers_disconnect_by_data (self->engine, self);
  g_hash_table_remove_all (self->plugins);
  g_hash_table_remove_all (self->actions);
  g_hash_table_remove_all (self->handlers);

  G_OBJECT_CLASS (valent_device_parent_class)->dispose (object);
}

static void
valent_device_finalize (GObject *object)
{
  ValentDevice *self = VALENT_DEVICE (object);

  g_clear_object (&self->data);
  g_clear_object (&self->settings);

  /* Properties */
  g_clear_pointer (&self->icon_name, g_free);
  g_clear_pointer (&self->id, g_free);
  g_clear_pointer (&self->name, g_free);
  g_clear_pointer (&self->type, g_free);
  g_clear_pointer (&self->incoming_capabilities, g_strfreev);
  g_clear_pointer (&self->outgoing_capabilities, g_strfreev);

  /* State */
  g_clear_object (&self->channel);

  /* Plugins */
  g_clear_pointer (&self->plugins, g_hash_table_unref);
  g_clear_pointer (&self->actions, g_hash_table_unref);
  g_clear_pointer (&self->handlers, g_hash_table_unref);
  g_clear_object (&self->menu);

  G_OBJECT_CLASS (valent_device_parent_class)->finalize (object);
}

static void
valent_device_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  ValentDevice *self = VALENT_DEVICE (object);

  switch (prop_id)
    {
    case PROP_CONNECTED:
      g_value_set_boolean (value, valent_device_get_connected (self));
      break;

    case PROP_DATA:
      g_value_take_object (value, valent_device_ref_data (self));
      break;

    case PROP_ICON_NAME:
      g_value_set_string (value, self->icon_name);
      break;

    case PROP_ID:
      g_value_set_string (value, self->id);
      break;

    case PROP_NAME:
      g_value_set_string (value, self->name);
      break;

    case PROP_PAIRED:
      g_value_set_boolean (value, self->paired);
      break;

    case PROP_STATE:
      g_value_set_flags (value, valent_device_get_state (self));
      break;

    case PROP_TYPE:
      g_value_set_string (value, self->type);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_device_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  ValentDevice *self = VALENT_DEVICE (object);

  switch (prop_id)
    {
    case PROP_DATA:
      self->data = g_value_dup_object (value);
      break;

    case PROP_ID:
      self->id = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_device_init (ValentDevice *self)
{
  GSimpleAction *action;

  /* Plugins */
  self->engine = valent_get_engine ();
  self->plugins = g_hash_table_new_full (NULL, NULL, NULL, device_plugin_free);
  self->handlers = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  self->actions = g_hash_table_new_full (g_str_hash,
                                          g_str_equal,
                                          g_free,
                                          g_object_unref);
  self->menu = g_menu_new ();

  /* Stock Actions */
  action = g_simple_action_new ("pair", NULL);
  g_signal_connect (action, "activate", G_CALLBACK (pair_action), self);
  g_hash_table_replace (self->actions, g_strdup ("pair"), action);

  action = g_simple_action_new ("unpair", NULL);
  g_signal_connect (action, "activate", G_CALLBACK (unpair_action), self);
  g_hash_table_replace (self->actions, g_strdup ("unpair"), action);
}

static void
valent_device_class_init (ValentDeviceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = valent_device_constructed;
  object_class->dispose = valent_device_dispose;
  object_class->finalize = valent_device_finalize;
  object_class->get_property = valent_device_get_property;
  object_class->set_property = valent_device_set_property;

  /**
   * ValentDevice:connected: (getter get_connected)
   *
   * Whether the device is connected.
   *
   * This property indicates whether the device has an active
   * [class@Valent.Channel] that has been authenticated. It does not imply the
   * the device has been paired, however.
   *
   * Since: 1.0
   */
  properties [PROP_CONNECTED] =
    g_param_spec_boolean ("connected",
                          "Connected",
                          "Whether the device is connected",
                          FALSE,
                          (G_PARAM_READABLE |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  /**
   * ValentDevice:data: (getter ref_data)
   *
   * The data context.
   *
   * This provides a relative point for files and settings, specific to the
   * device in question.
   *
   * Since: 1.0
   */
  properties [PROP_DATA] =
    g_param_spec_object ("data",
                         "Data Manager",
                         "The data context",
                         VALENT_TYPE_DATA,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentDevice:icon-name: (getter get_icon_name)
   *
   * A symbolic icon name for the device.
   *
   * See [property@Valent.Device:type].
   *
   * Since: 1.0
   */
  properties [PROP_ICON_NAME] =
    g_param_spec_string ("icon-name",
                         "Icon Name",
                         "Icon name representing the device",
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentDevice:id: (getter get_id)
   *
   * A unique ID for the device.
   *
   * By convention, the single source of truth for a device ID in KDE Connect is
   * the common name of its TLS certificate. It is not well-defined how this ID
   * is generated, however.
   *
   * Since: 1.0
   */
  properties [PROP_ID] =
    g_param_spec_string ("id",
                         "Id",
                         "Unique id for the device",
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentDevice:name: (getter get_name)
   *
   * A display name for the device.
   *
   * Since: 1.0
   */
  properties [PROP_NAME] =
    g_param_spec_string ("name",
                         "Name",
                         "Name representing the device",
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentDevice:paired: (getter get_paired)
   *
   * Whether the device is paired.
   *
   * This property indicates whether the device is paired with respect to the
   * KDE Connect protocol and may be unrelated to the underlying transport
   * protocol (eg. Bluetooth).
   *
   * Since: 1.0
   */
  properties [PROP_PAIRED] =
    g_param_spec_boolean ("paired",
                          "Paired",
                          "Whether the device is paired",
                          FALSE,
                          (G_PARAM_READABLE |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  /**
   * ValentDevice:state: (getter get_state)
   *
   * The state of the device.
   *
   * This is intended to provide more granular information about the state than
   * [property@Valent.Device:connected] or [property@Valent.Device:paired].
   *
   * Since: 1.0
   */
  properties [PROP_STATE] =
    g_param_spec_flags ("state",
                        "State",
                        "State of device",
                        VALENT_TYPE_DEVICE_STATE,
                        VALENT_DEVICE_STATE_NONE,
                        (G_PARAM_READABLE |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  /**
   * ValentDevice:type:
   *
   * A string hint, indicating the form-factor of the device.
   *
   * Known values include `desktop`, `laptop`, `smartphone`, `tablet` and `tv`.
   *
   * This is generally only useful for things like selecting an icon, since the
   * device will describe its capabilities by other means.
   *
   * Since: 1.0
   */
  properties [PROP_TYPE] =
    g_param_spec_string ("type",
                         "Type",
                         "Type of device (eg. phone, tablet, laptop)",
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);

  /**
   * ValentDevice::plugin-added:
   * @device: a #ValentDevice
   * @plugin_info: a #PeasPluginInfo
   *
   * Emitted when a supported plugin has been loaded by the device.
   *
   * This could be a result of the [class@Peas.Engine] loading a plugin that
   * provides a [class@Valent.DevicePlugin] extension or the device receiving an
   * identity packet indicating the capabilities of the device have changed.
   *
   * Since: 1.0
   */
  signals [PLUGIN_ADDED] =
    g_signal_new ("plugin-added",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__BOXED,
                  G_TYPE_NONE, 1, PEAS_TYPE_PLUGIN_INFO);
  g_signal_set_va_marshaller (signals [PLUGIN_ADDED],
                              G_TYPE_FROM_CLASS (klass),
                              g_cclosure_marshal_VOID__BOXEDv);

  /**
   * ValentDevice::plugin-removed:
   * @device: a #ValentDevice
   * @plugin_info: a #PeasPluginInfo
   *
   * Emitted when a supported plugin has been unloaded by the device.
   *
   * This could be a result of the [class@Peas.Engine] unloading a plugin that
   * provided a [class@Valent.DevicePlugin] extension or the device receiving an
   * identity packet indicating the capabilities of the device have changed.
   *
   * Since: 1.0
   */
  signals [PLUGIN_REMOVED] =
    g_signal_new ("plugin-removed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__BOXED,
                  G_TYPE_NONE, 1, PEAS_TYPE_PLUGIN_INFO);
  g_signal_set_va_marshaller (signals [PLUGIN_REMOVED],
                              G_TYPE_FROM_CLASS (klass),
                              g_cclosure_marshal_VOID__BOXEDv);
}

/**
 * valent_device_new:
 * @identity: a KDE Connect identity packet
 *
 * Construct a new device for @identity.
 *
 * Returns: (transfer full) (nullable): a new #ValentDevice
 *
 * Since: 1.0
 */
ValentDevice *
valent_device_new (JsonNode *identity)
{
  ValentDevice *ret;
  const char *id;

  g_return_val_if_fail (VALENT_IS_PACKET (identity), NULL);

  if (!valent_packet_get_string (identity, "deviceId", &id))
    {
      g_critical ("%s(): missing \"deviceId\" field", G_STRFUNC);
      return NULL;
    }

  ret = g_object_new (VALENT_TYPE_DEVICE,
                      "id", id,
                      NULL);
  valent_device_handle_identity (ret, identity);

  return ret;
}

/**
 * valent_device_new_full:
 * @identity: a KDE Connect identity packet
 * @data: (nullable): the data context
 *
 * Construct a new device for @identity.
 *
 * Returns: (transfer full) (nullable): a new #ValentDevice
 *
 * Since: 1.0
 */
ValentDevice *
valent_device_new_full (JsonNode   *identity,
                        ValentData *data)
{
  ValentDevice *ret;
  const char *id;

  g_return_val_if_fail (VALENT_IS_PACKET (identity), NULL);

  if (!valent_packet_get_string (identity, "deviceId", &id))
    {
      g_critical ("%s(): missing \"deviceId\" field", G_STRFUNC);
      return NULL;
    }

  ret = g_object_new (VALENT_TYPE_DEVICE,
                      "id",   id,
                      "data", data,
                      NULL);
  valent_device_handle_identity (ret, identity);

  return ret;
}

static void
valent_device_queue_packet_cb (ValentChannel *channel,
                               GAsyncResult  *result,
                               ValentDevice  *device)
{
  g_autoptr (GError) error = NULL;

  g_assert (VALENT_IS_CHANNEL (channel));
  g_assert (VALENT_IS_DEVICE (device));

  if (!valent_channel_write_packet_finish (channel, result, &error))
    {
      VALENT_NOTE ("%s: %s", device->name, error->message);

      valent_object_lock (VALENT_OBJECT (device));
      if (device->channel == channel)
        valent_device_set_channel (device, NULL);
      valent_object_unlock (VALENT_OBJECT (device));
    }

  g_object_unref (device);
}

/**
 * valent_device_queue_packet:
 * @device: a #ValentDevice
 * @packet: a KDE Connect packet
 *
 * Queue a KDE Connect packet to be sent to the device.
 *
 * If @device is disconnected or unpaired when this method is called, a warning
 * or critical will be logged, respectively.
 *
 * See [method@Valent.Device.send_packet] for a failable and cancellable variant
 * of this method.
 *
 * Since: 1.0
 */
void
valent_device_queue_packet (ValentDevice *device,
                            JsonNode     *packet)
{
  g_return_if_fail (VALENT_IS_DEVICE (device));
  g_return_if_fail (VALENT_IS_PACKET (packet));

  valent_object_lock (VALENT_OBJECT (device));

  if G_UNLIKELY (device->channel == NULL)
    {
      g_warning ("%s(): %s is disconnected, discarding \"%s\"",
                 G_STRFUNC,
                 device->name,
                 valent_packet_get_type (packet));
      valent_object_unlock (VALENT_OBJECT (device));
      return;
    }

  if G_UNLIKELY (!device->paired)
    {
      g_critical ("%s(): %s is unpaired, discarding \"%s\"",
                  G_STRFUNC,
                  device->name,
                  valent_packet_get_type (packet));
      valent_object_unlock (VALENT_OBJECT (device));
      return;
    }

  VALENT_JSON (packet, device->name);
  valent_channel_write_packet (device->channel,
                               packet,
                               NULL,
                               (GAsyncReadyCallback)valent_device_queue_packet_cb,
                               g_object_ref (device));

  valent_object_unlock (VALENT_OBJECT (device));
}

static void
valent_device_send_packet_cb (ValentChannel *channel,
                              GAsyncResult  *result,
                              gpointer       user_data)
{
  g_autoptr (GTask) task = G_TASK (user_data);
  ValentDevice *device = g_task_get_source_object (task);
  GError *error = NULL;

  g_assert (VALENT_IS_DEVICE (device));

  if (!valent_channel_write_packet_finish (channel, result, &error))
    {
      VALENT_NOTE ("%s: %s", device->name, error->message);

      g_task_return_error (task, error);

      valent_object_lock (VALENT_OBJECT (device));
      if (device->channel == channel)
        valent_device_set_channel (device, NULL);
      valent_object_unlock (VALENT_OBJECT (device));
    }
  else
    {
      g_task_return_boolean (task, TRUE);
    }
}

/**
 * valent_device_send_packet:
 * @device: a #ValentDevice
 * @packet: a KDE Connect packet
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): a #GAsyncReadyCallback
 * @user_data: (closure): user supplied data
 *
 * Send a KDE Connect packet to the device.
 *
 * Call [method@Valent.Device.send_packet_finish] to get the result.
 *
 * If @device is disconnected or unpaired when this method is called,
 * %G_IO_ERROR_NOT_CONNECTED or %G_IO_ERROR_PERMISSION_DENIED will be set on the
 * result, respectively.
 *
 * Since: 1.0
 */
void
valent_device_send_packet (ValentDevice        *device,
                           JsonNode            *packet,
                           GCancellable        *cancellable,
                           GAsyncReadyCallback  callback,
                           gpointer             user_data)
{
  g_autoptr (GTask) task = NULL;

  g_return_if_fail (VALENT_IS_DEVICE (device));
  g_return_if_fail (VALENT_IS_PACKET (packet));

  valent_object_lock (VALENT_OBJECT (device));

  if G_UNLIKELY (device->channel == NULL)
    {
      valent_object_unlock (VALENT_OBJECT (device));
      return g_task_report_new_error (device,
                                      callback,
                                      user_data,
                                      valent_device_send_packet,
                                      G_IO_ERROR,
                                      G_IO_ERROR_NOT_CONNECTED,
                                      "%s is disconnected", device->name);
    }

  if G_UNLIKELY (!device->paired)
    {
      valent_object_unlock (VALENT_OBJECT (device));
      return g_task_report_new_error (device,
                                      callback,
                                      user_data,
                                      valent_device_send_packet,
                                      G_IO_ERROR,
                                      G_IO_ERROR_PERMISSION_DENIED,
                                      "%s is unpaired", device->name);
    }

  task = g_task_new (device, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_device_send_packet);

  VALENT_JSON (packet, device->name);
  valent_channel_write_packet (device->channel,
                               packet,
                               cancellable,
                               (GAsyncReadyCallback)valent_device_send_packet_cb,
                               g_steal_pointer (&task));

  valent_object_unlock (VALENT_OBJECT (device));
}

/**
 * valent_device_send_packet_finish:
 * @device: a #ValentDevice
 * @result: a #GAsyncResult
 * @error: (nullable): a #GError
 *
 * Finish an operation started by [method@Valent.Device.send_packet].
 *
 * Returns: %TRUE if successful, or %FALSE with @error set
 *
 * Since: 1.0
 */
gboolean
valent_device_send_packet_finish (ValentDevice  *device,
                                  GAsyncResult  *result,
                                  GError       **error)
{
  g_return_val_if_fail (VALENT_IS_DEVICE (device), FALSE);
  g_return_val_if_fail (g_task_is_valid (result, device), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

/**
 * valent_device_ref_channel:
 * @device: a #ValentDevice
 *
 * Get the active channel.
 *
 * Returns: (transfer full) (nullable): a #ValentChannel, or %NULL if disconnected
 *
 * Since: 1.0
 */
ValentChannel *
valent_device_ref_channel (ValentDevice *device)
{
  ValentChannel *ret = NULL;

  g_return_val_if_fail (VALENT_IS_DEVICE (device), NULL);

  valent_object_lock (VALENT_OBJECT (device));
  if (device->channel != NULL)
    ret = g_object_ref (device->channel);
  valent_object_unlock (VALENT_OBJECT (device));

  return ret;
}

static void
read_packet_cb (ValentChannel *channel,
                GAsyncResult  *result,
                ValentDevice  *device)
{
  g_autoptr (GError) error = NULL;
  g_autoptr (JsonNode) packet = NULL;

  g_assert (VALENT_IS_CHANNEL (channel));
  g_assert (VALENT_IS_DEVICE (device));

  packet = valent_channel_read_packet_finish (channel, result, &error);

  /* On success, queue another read before handling the packet */
  if (packet != NULL)
    {
      valent_channel_read_packet (channel,
                                  NULL,
                                  (GAsyncReadyCallback)read_packet_cb,
                                  g_object_ref (device));

      valent_device_handle_packet (device, packet);
    }

  /* On failure, drop our reference if it's still the active channel */
  else
    {
      VALENT_NOTE ("%s: %s", device->name, error->message);

      valent_object_lock (VALENT_OBJECT (device));
      if (device->channel == channel)
        valent_device_set_channel (device, NULL);
      valent_object_unlock (VALENT_OBJECT (device));
    }

  g_object_unref (device);
}

/**
 * valent_device_set_channel:
 * @device: A #ValentDevice
 * @channel: (nullable): A #ValentChannel
 *
 * Sets the active channel.
 *
 * Since: 1.0
 */
void
valent_device_set_channel (ValentDevice  *device,
                           ValentChannel *channel)
{
  gboolean connected;

  g_return_if_fail (VALENT_IS_DEVICE (device));
  g_return_if_fail (channel == NULL || VALENT_IS_CHANNEL (channel));

  valent_object_lock (VALENT_OBJECT (device));

  if (device->channel == channel)
    {
      valent_object_unlock (VALENT_OBJECT (device));
      return;
    }

  /* If there's an active channel, close it asynchronously and drop our
   * reference so the task holds the final reference. */
  if ((connected = device->channel != NULL))
    {
      valent_channel_close_async (device->channel, NULL, NULL, NULL);
      g_clear_object (&device->channel);
    }

  /* If there's a new channel handle the peer identity and queue the first read
   * before calling valent_device_set_connected(). */
  if (g_set_object (&device->channel, channel))
    {
      JsonNode *peer_identity;

      /* Handle the peer identity packet */
      peer_identity = valent_channel_get_peer_identity (channel);
      valent_device_handle_identity (device, peer_identity);

      /* Start receiving packets */
      valent_channel_read_packet (channel,
                                  NULL,
                                  (GAsyncReadyCallback)read_packet_cb,
                                  g_object_ref (device));
    }

  valent_object_unlock (VALENT_OBJECT (device));

  /* If the connected state changed, update the plugins and notify */
  if (valent_device_get_connected (device) == connected)
    return;

  valent_device_update_plugins (device);
  valent_object_notify_by_pspec (G_OBJECT (device), properties [PROP_CONNECTED]);
  valent_object_notify_by_pspec (G_OBJECT (device), properties [PROP_STATE]);
}

/**
 * valent_device_get_connected: (get-property connected)
 * @device: a #ValentDevice
 *
 * Get whether the device is connected.
 *
 * Returns: %TRUE if the device has an active connection.
 *
 * Since: 1.0
 */
gboolean
valent_device_get_connected (ValentDevice *device)
{
  gboolean ret;

  g_return_val_if_fail (VALENT_IS_DEVICE (device), FALSE);

  valent_object_lock (VALENT_OBJECT (device));
  ret = device->channel != NULL;
  valent_object_unlock (VALENT_OBJECT (device));

  return ret;
}

/**
 * valent_device_ref_data: (get-property data)
 * @device: a #ValentDevice
 *
 * Get the data context for the device.
 *
 * Returns: (transfer full): a #ValentData
 *
 * Since: 1.0
 */
ValentData *
valent_device_ref_data (ValentDevice *device)
{
  ValentData *ret = NULL;

  g_return_val_if_fail (VALENT_IS_DEVICE (device), NULL);

  valent_object_lock (VALENT_OBJECT (device));
  if (device->data != NULL)
    ret = g_object_ref (device->data);
  valent_object_unlock (VALENT_OBJECT (device));

  return ret;
}

/**
 * valent_device_get_icon_name: (get-property icon-name)
 * @device: a #ValentDevice
 *
 * Get the symbolic icon name.
 *
 * Returns: (transfer none): the icon name.
 *
 * Since: 1.0
 */
const char *
valent_device_get_icon_name (ValentDevice *device)
{
  g_return_val_if_fail (VALENT_IS_DEVICE (device), "computer-symbolic");

  return device->icon_name;
}

/**
 * valent_device_get_id: (get-property id)
 * @device: a #ValentDevice
 *
 * Get the unique ID.
 *
 * Returns: (transfer none): the device id.
 *
 * Since: 1.0
 */
const char *
valent_device_get_id (ValentDevice *device)
{
  g_return_val_if_fail (VALENT_IS_DEVICE (device), NULL);
  g_return_val_if_fail (device->id != NULL, NULL);

  return device->id;
}

/**
 * valent_device_get_menu:
 * @device: a #ValentDevice
 *
 * Get the [class@Gio.MenuModel] of the device.
 *
 * Plugins may add items and submenus to this when they want to expose actions
 * with presentation details like a label or icon.
 *
 * Returns: (transfer none): a #GMenuModel
 *
 * Since: 1.0
 */
GMenuModel *
valent_device_get_menu (ValentDevice *device)
{
  g_return_val_if_fail (VALENT_IS_DEVICE (device), NULL);

  return G_MENU_MODEL (device->menu);
}

/**
 * valent_device_get_name:
 * @device: a #ValentDevice
 *
 * Get the display name of the device.
 *
 * Returns: (transfer none) (nullable): a display name, or %NULL if unset
 *
 * Since: 1.0
 */
const char *
valent_device_get_name (ValentDevice *device)
{
  g_return_val_if_fail (VALENT_IS_DEVICE (device), NULL);

  return device->name;
}

/**
 * valent_device_get_paired: (get-property paired)
 * @device: a #ValentDevice
 *
 * Get whether the device is paired.
 *
 * Returns: %TRUE if the device is paired, or %FALSE if unpaired
 *
 * Since: 1.0
 */
gboolean
valent_device_get_paired (ValentDevice *device)
{
  gboolean ret;

  g_return_val_if_fail (VALENT_IS_DEVICE (device), FALSE);

  valent_object_lock (VALENT_OBJECT (device));
  ret = device->paired;
  valent_object_unlock (VALENT_OBJECT (device));

  return ret;
}

/**
 * valent_device_set_paired: (set-property paired)
 * @device: a #ValentDevice
 * @paired: %TRUE if paired, %FALSE if unpaired
 *
 * Set the paired state of the device.
 *
 * NOTE: since valent_device_update_plugins() will be called as a side effect,
 * this must be called after valent_device_send_pair().
 *
 * Since: 1.0
 */
void
valent_device_set_paired (ValentDevice *device,
                          gboolean      paired)
{
  g_assert (VALENT_IS_DEVICE (device));

  valent_object_lock (VALENT_OBJECT (device));

  /* If nothing's changed, only reset pending pair timeouts */
  valent_device_reset_pair (device);

  if (device->paired == paired)
    {
      valent_object_unlock (VALENT_OBJECT (device));
      return;
    }

  /* FIXME: If we're connected store/clear connection data */
  if (paired && device->channel != NULL)
    valent_channel_store_data (device->channel, device->data);
  else if (!paired)
    valent_data_clear_data (device->data);

  device->paired = paired;
  g_settings_set_boolean (device->settings, "paired", device->paired);

  valent_object_unlock (VALENT_OBJECT (device));

  /* Update plugins and notify */
  valent_device_update_plugins (device);
  valent_object_notify_by_pspec (G_OBJECT (device), properties [PROP_PAIRED]);
  valent_object_notify_by_pspec (G_OBJECT (device), properties [PROP_STATE]);
}

/**
 * valent_device_get_plugins:
 * @device: a #ValentDevice
 *
 * Get a list of the loaded plugins.
 *
 * Returns: (transfer container) (element-type Peas.PluginInfo): a #GPtrArray
 *
 * Since: 1.0
 */
GPtrArray *
valent_device_get_plugins (ValentDevice *device)
{
  GHashTableIter iter;
  gpointer info;
  GPtrArray *plugins;

  g_return_val_if_fail (VALENT_IS_DEVICE (device), NULL);

  plugins = g_ptr_array_new ();

  g_hash_table_iter_init (&iter, device->plugins);

  while (g_hash_table_iter_next (&iter, &info, NULL))
    g_ptr_array_add (plugins, info);

  return plugins;
}

/**
 * valent_device_get_state: (get-property state)
 * @device: a #ValentDevice
 *
 * Get the state of the device.
 *
 * Returns: #ValentDeviceStateFlags describing the state of the device
 *
 * Since: 1.0
 */
ValentDeviceState
valent_device_get_state (ValentDevice *device)
{
  ValentDeviceState state = VALENT_DEVICE_STATE_NONE;

  g_return_val_if_fail (VALENT_IS_DEVICE (device), state);

  valent_object_lock (VALENT_OBJECT (device));

  if (device->channel != NULL)
    state |= VALENT_DEVICE_STATE_CONNECTED;

  if (device->paired)
    state |= VALENT_DEVICE_STATE_PAIRED;

  if (device->incoming_pair > 0)
    state |= VALENT_DEVICE_STATE_PAIR_INCOMING;

  if (device->outgoing_pair > 0)
    state |= VALENT_DEVICE_STATE_PAIR_OUTGOING;

  valent_object_unlock (VALENT_OBJECT (device));

  return state;
}

/**
 * valent_device_handle_packet:
 * @device: a #ValentDevice
 * @packet: a KDE Connect packet
 *
 * Handle a packet from the remote device.
 *
 * Handle @packet as a message from the remote device. Pair packets are handled
 * by @device internally, while all others will be passed to plugins which claim
 * to support the @packet type.
 *
 * Since: 1.0
 */
void
valent_device_handle_packet (ValentDevice *device,
                             JsonNode     *packet)
{
  ValentDevicePlugin *handler;
  const char *type;

  g_assert (VALENT_IS_DEVICE (device));
  g_assert (VALENT_IS_PACKET (packet));

  VALENT_JSON (packet, device->name);

  type = valent_packet_get_type (packet);

  /* This is the only packet type an unpaired device can send or receive */
  if G_UNLIKELY (g_strcmp0 (type, "kdeconnect.pair") == 0)
    valent_device_handle_pair (device, packet);

  /* If unpaired, any other packet is ignored and the remote device notified */
  else if G_UNLIKELY (!device->paired)
    valent_device_send_pair (device, FALSE);

  /* If paired, try to find a plugin that can handle the packet type */
  else if ((handler = g_hash_table_lookup (device->handlers, type)))
    valent_device_plugin_handle_packet (handler, type, packet);

  else
    g_debug ("%s: Unsupported packet \"%s\"", device->name, type);
}

/**
 * valent_device_new_download_file:
 * @device: a #ValentDevice
 * @filename: (type filename): a filename
 * @unique: whether to ensure a unique file
 *
 * Get a new [iface@Gio.File] for in the download directory of the device.
 *
 * If @unique is %TRUE, the returned file is guaranteed not to be an existing
 * filename by appending `(#)`.
 *
 * Returns: (transfer full) (nullable): a #GFile
 *
 * Since: 1.0
 */
GFile *
valent_device_new_download_file (ValentDevice *device,
                                 const char   *filename,
                                 gboolean      unique)
{
  g_autofree char *dirname = NULL;

  g_return_val_if_fail (VALENT_IS_DEVICE (device), NULL);
  g_return_val_if_fail (filename != NULL, NULL);

  dirname = g_settings_get_string (device->settings, "download-folder");

  if (strlen (dirname) == 0)
    {
      g_clear_pointer (&dirname, g_free);
      dirname = valent_data_get_directory (G_USER_DIRECTORY_DOWNLOAD);
    }
  else if (g_mkdir_with_parents (dirname, 0700) == -1)
    {
      int error = errno;

      g_critical ("%s(): creating \"%s\": %s",
                  G_STRFUNC,
                  dirname,
                  g_strerror (error));
    }

  return valent_data_get_file (dirname, filename, unique);
}

/**
 * valent_device_reload_plugins:
 * @device: a #ValentDevice
 *
 * Reload all plugins.
 *
 * Check each available plugin and load or unload them if the required
 * capabilities have changed.
 *
 * Since: 1.0
 */
static void
valent_device_reload_plugins (ValentDevice *device)
{
  const GList *plugins = NULL;

  g_assert (VALENT_IS_DEVICE (device));

  plugins = peas_engine_get_plugin_list (device->engine);

  for (const GList *iter = plugins; iter; iter = iter->next)
    {
      if (valent_device_supports_plugin (device, iter->data))
        on_load_plugin (device->engine, iter->data, device);
      else
        on_unload_plugin (device->engine, iter->data, device);
    }
}

/**
 * valent_device_update_plugins:
 * @device: a #ValentDevice
 *
 * Update all plugins.
 *
 * Call [method@Valent.DevicePlugin.update_state] on each enabled plugin.
 *
 * Since: 1.0
 */
static void
valent_device_update_plugins (ValentDevice *device)
{
  ValentDeviceState state = VALENT_DEVICE_STATE_NONE;
  GHashTableIter iter;
  DevicePlugin *plugin;

  g_assert (VALENT_IS_DEVICE (device));

  state = valent_device_get_state (device);

  g_hash_table_iter_init (&iter, device->plugins);

  while (g_hash_table_iter_next (&iter, NULL, (void **)&plugin))
    {
      if (plugin->extension == NULL)
        continue;

      valent_device_plugin_update_state (VALENT_DEVICE_PLUGIN (plugin->extension),
                                         state);
    }
}

/**
 * valent_device_supports_plugin:
 * @device: a #ValentDevice
 * @info: a #PeasPluginInfo
 *
 * Check if @device supports the plugin described by @info.
 *
 * Returns: %TRUE if supported, or %FALSE if not
 *
 * Since: 1.0
 */
static gboolean
valent_device_supports_plugin (ValentDevice   *device,
                               PeasPluginInfo *info)
{
  const char **device_incoming;
  const char **device_outgoing;
  const char *in_str, *out_str;

  g_assert (VALENT_IS_DEVICE (device));
  g_assert (info != NULL);

  if (!peas_engine_provides_extension (device->engine,
                                       info,
                                       VALENT_TYPE_DEVICE_PLUGIN))
    return FALSE;

  /* Packet-less plugins aren't dependent on device capabilities */
  in_str = peas_plugin_info_get_external_data (info, "IncomingCapabilities");
  out_str = peas_plugin_info_get_external_data (info, "OutgoingCapabilities");

  if (in_str == NULL && out_str == NULL)
    return TRUE;

  /* Device hasn't supplied an identity packet yet */
  device_incoming = (const char **)device->incoming_capabilities;
  device_outgoing = (const char **)device->outgoing_capabilities;

  if (device_incoming == NULL || device_outgoing == NULL)
    return FALSE;

  /* Check if outgoing from plugin matches incoming from device */
  if (out_str != NULL)
    {
      g_auto (GStrv) plugin_outgoing = NULL;

      plugin_outgoing = g_strsplit(out_str, ";", -1);

      for (int i = 0; plugin_outgoing[i]; i++)
        {
          if (g_strv_contains (device_incoming, plugin_outgoing[i]))
            return TRUE;
        }
    }

  /* Check if incoming from plugin matches outgoing from device */
  if (in_str != NULL)
    {
      g_auto (GStrv) plugin_incoming = NULL;

      plugin_incoming = g_strsplit(in_str, ";", -1);

      for (int i = 0; plugin_incoming[i]; i++)
        {
          if (g_strv_contains (device_outgoing, plugin_incoming[i]))
            return TRUE;
        }
    }

  return FALSE;
}

