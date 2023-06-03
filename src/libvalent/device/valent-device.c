// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-device"

#include "config.h"

#include <glib/gi18n.h>
#include <gio/gio.h>
#include <libvalent-core.h>

#include "valent-device-enums.h"

#include "../core/valent-component-private.h"
#include "valent-channel.h"
#include "valent-device.h"
#include "valent-device-plugin.h"
#include "valent-device-private.h"
#include "valent-packet.h"

#define DEVICE_TYPE_DESKTOP  "desktop"
#define DEVICE_TYPE_LAPTOP   "laptop"
#define DEVICE_TYPE_PHONE    "phone"
#define DEVICE_TYPE_TABLET   "tablet"
#define DEVICE_TYPE_TV       "tv"

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

  ValentContext  *context;
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

G_DEFINE_FINAL_TYPE_WITH_CODE (ValentDevice, valent_device, VALENT_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_ACTION_GROUP, g_action_group_iface_init))


enum {
  PROP_0,
  PROP_CONTEXT,
  PROP_ICON_NAME,
  PROP_ID,
  PROP_NAME,
  PROP_PLUGINS,
  PROP_STATE,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };


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
static void
device_plugin_free (gpointer data)
{
  ValentPlugin *plugin = data;

  /* `::action-removed` needs to be emitted before the plugin is freed */
  if (plugin->extension != NULL)
    valent_object_destroy (VALENT_OBJECT (plugin->extension));

  g_clear_pointer (&plugin, valent_plugin_free);
}

static void
on_plugin_action_added (GActionGroup *action_group,
                        const char   *action_name,
                        ValentPlugin *plugin)
{
  ValentDevice *self = VALENT_DEVICE (plugin->parent);
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
  g_action_group_action_added (G_ACTION_GROUP (plugin->parent), full_name);
}

static void
on_plugin_action_enabled_changed (GActionGroup *action_group,
                                  const char   *action_name,
                                  gboolean      enabled,
                                  ValentPlugin *plugin)
{
  g_autofree char *full_name = NULL;

  full_name = g_strdup_printf ("%s.%s",
                               peas_plugin_info_get_module_name (plugin->info),
                               action_name);
  g_action_group_action_enabled_changed (G_ACTION_GROUP (plugin->parent),
                                         full_name,
                                         enabled);
}

static void
on_plugin_action_removed (GActionGroup *action_group,
                          const char   *action_name,
                          ValentPlugin *plugin)
{
  ValentDevice *self = VALENT_DEVICE (plugin->parent);
  g_autofree char *full_name = NULL;

  full_name = g_strdup_printf ("%s.%s",
                               peas_plugin_info_get_module_name (plugin->info),
                               action_name);

  g_action_group_action_removed (G_ACTION_GROUP (plugin->parent), full_name);
  g_hash_table_remove (self->actions, full_name);
}

static void
on_plugin_action_state_changed (GActionGroup *action_group,
                                const char   *action_name,
                                GVariant     *value,
                                ValentPlugin *plugin)
{
  g_autofree char *full_name = NULL;

  full_name = g_strdup_printf ("%s.%s",
                               peas_plugin_info_get_module_name (plugin->info),
                               action_name);
  g_action_group_action_state_changed (G_ACTION_GROUP (plugin->parent),
                                       full_name,
                                       value);
}

static void
valent_device_enable_plugin (ValentDevice *device,
                             ValentPlugin *plugin)
{
  g_auto (GStrv) actions = NULL;
  const char *incoming = NULL;

  g_assert (VALENT_IS_DEVICE (device));
  g_assert (plugin != NULL);

  /* Instantiate the plugin */
  plugin->extension = peas_engine_create_extension (device->engine,
                                                    plugin->info,
                                                    VALENT_TYPE_DEVICE_PLUGIN,
                                                    "context", plugin->context,
                                                    "object",  plugin->parent,
                                                    NULL);
  g_return_if_fail (G_IS_OBJECT (plugin->extension));

  /* Register packet handlers */
  incoming = peas_plugin_info_get_external_data (plugin->info,
                                                 "DevicePluginIncoming");

  if (incoming != NULL)
    {
      g_auto (GStrv) capabilities = NULL;

      capabilities = g_strsplit (incoming, ";", -1);

      for (unsigned int i = 0; capabilities[i] != NULL; i++)
        {
          GPtrArray *handlers = NULL;
          const char *type = capabilities[i];

          if ((handlers = g_hash_table_lookup (device->handlers, type)) == NULL)
            {
              handlers = g_ptr_array_new ();
              g_hash_table_insert (device->handlers, g_strdup (type), handlers);
            }

          g_ptr_array_add (handlers, plugin->extension);
        }
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
  valent_device_plugin_update_state (VALENT_DEVICE_PLUGIN (plugin->extension),
                                     valent_device_get_state (device));
}

static void
valent_device_disable_plugin (ValentDevice *device,
                              ValentPlugin *plugin)
{
  g_auto (GStrv) actions = NULL;
  const char *incoming = NULL;

  g_assert (VALENT_IS_DEVICE (device));
  g_assert (plugin != NULL);
  g_return_if_fail (G_IS_OBJECT (plugin->extension));

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
  incoming = peas_plugin_info_get_external_data (plugin->info,
                                                 "DevicePluginIncoming");

  if (incoming != NULL)
    {
      g_auto (GStrv) capabilities = NULL;

      capabilities = g_strsplit (incoming, ";", -1);

      for (unsigned int i = 0; capabilities[i] != NULL; i++)
        {
          const char *type = capabilities[i];
          GPtrArray *handlers = NULL;

          if ((handlers = g_hash_table_lookup (device->handlers, type)) == NULL)
            continue;

          if (g_ptr_array_remove (handlers, plugin->extension) && handlers->len == 0)
            g_hash_table_remove (device->handlers, type);
        }
    }

  /* `::action-removed` needs to be emitted before the plugin is freed */
  valent_object_destroy (VALENT_OBJECT (plugin->extension));
  g_clear_object (&plugin->extension);
}

static void
on_plugin_enabled_changed (ValentPlugin *plugin)
{
  g_assert (plugin != NULL);
  g_assert (VALENT_IS_DEVICE (plugin->parent));

  if (valent_plugin_get_enabled (plugin))
    valent_device_enable_plugin (plugin->parent, plugin);
  else
    valent_device_disable_plugin (plugin->parent, plugin);
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

  g_object_notify_by_pspec (G_OBJECT (device), properties [PROP_STATE]);

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
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonNode) packet = NULL;
  g_autoptr (GCancellable) cancellable = NULL;

  g_assert (VALENT_IS_DEVICE (device));

  valent_object_lock (VALENT_OBJECT (device));

  if (device->channel == NULL)
    {
      valent_object_unlock (VALENT_OBJECT (device));
      return;
    }

  valent_packet_init (&builder, "kdeconnect.pair");
  json_builder_set_member_name (builder, "pair");
  json_builder_add_boolean_value (builder, pair);
  packet = valent_packet_end (&builder);

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
  g_object_notify_by_pspec (G_OBJECT (device), properties [PROP_STATE]);
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

  if (valent_set_string (&device->name, device_name))
    g_object_notify_by_pspec (G_OBJECT (device), properties [PROP_NAME]);

  /* Device Type */
  if (!valent_packet_get_string (packet, "deviceType", &device_type))
    device_type = DEVICE_TYPE_DESKTOP;

  if (valent_set_string (&device->type, device_type))
    {
      const char *device_icon = "computer-symbolic";

      if (g_str_equal (device_type, DEVICE_TYPE_DESKTOP))
        device_icon = "computer-symbolic";
      else if (g_str_equal (device_type, DEVICE_TYPE_LAPTOP))
        device_icon = "laptop-symbolic";
      else if (g_str_equal (device_type, DEVICE_TYPE_PHONE))
        device_icon = "phone-symbolic";
      else if (g_str_equal (device_type, DEVICE_TYPE_TABLET))
        device_icon = "tablet-symbolic";
      else if (g_str_equal (device_type, DEVICE_TYPE_TV))
        device_icon = "tv-symbolic";

      if (valent_set_string (&device->icon_name, device_icon))
        g_object_notify_by_pspec (G_OBJECT (device), properties [PROP_ICON_NAME]);
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
                ValentDevice   *self)
{
  ValentPlugin *plugin;

  g_assert (PEAS_IS_ENGINE (engine));
  g_assert (info != NULL);
  g_assert (VALENT_IS_DEVICE (self));

  if (!valent_device_supports_plugin (self, info))
    return;

  if (g_hash_table_contains (self->plugins, info))
    return;

  VALENT_NOTE ("%s: %s",
               self->name,
               peas_plugin_info_get_module_name (info));

  /* Register the plugin & data (hash tables are ref owners) */
  plugin = valent_plugin_new (self, self->context, info,
                              G_CALLBACK (on_plugin_enabled_changed));
  g_hash_table_insert (self->plugins, info, plugin);

  if (valent_plugin_get_enabled (plugin))
    valent_device_enable_plugin (self, plugin);

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PLUGINS]);
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
  g_object_notify_by_pspec (G_OBJECT (device), properties [PROP_PLUGINS]);
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

      g_object_notify_by_pspec (G_OBJECT (device), properties [PROP_STATE]);
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
  const GList *plugins = NULL;

  /* We must at least have a device ID */
  g_assert (self->id != NULL);

  /* Context */
  if (self->context == NULL)
    self->context = valent_context_new (NULL, "device", self->id);

  /* GSettings*/
  path = g_strdup_printf ("/ca/andyholmes/valent/device/%s/", self->id);
  self->settings = g_settings_new_with_path ("ca.andyholmes.Valent.Device", path);
  self->paired = g_settings_get_boolean (self->settings, "paired");

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

  g_clear_object (&self->context);
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
    case PROP_CONTEXT:
      g_value_set_object (value, self->context);
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

    case PROP_PLUGINS:
      g_value_take_boxed (value, valent_device_get_plugins (self));
      break;

    case PROP_STATE:
      g_value_set_flags (value, valent_device_get_state (self));
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
    case PROP_CONTEXT:
      self->context = g_value_dup_object (value);
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
  self->engine = valent_get_plugin_engine ();
  self->plugins = g_hash_table_new_full (NULL, NULL, NULL, device_plugin_free);
  self->handlers = g_hash_table_new_full (g_str_hash,
                                          g_str_equal,
                                          g_free,
                                          (GDestroyNotify)g_ptr_array_unref);
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
   * ValentDevice:context: (getter get_context)
   *
   * The data context.
   *
   * Since: 1.0
   */
  properties [PROP_CONTEXT] =
    g_param_spec_object ("context", NULL, NULL,
                         VALENT_TYPE_CONTEXT,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentDevice:icon-name: (getter get_icon_name)
   *
   * A symbolic icon name for the device.
   *
   * Since: 1.0
   */
  properties [PROP_ICON_NAME] =
    g_param_spec_string ("icon-name", NULL, NULL,
                         "computer-symbolic",
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
    g_param_spec_string ("id", NULL, NULL,
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
    g_param_spec_string ("name", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentDevice:plugins: (getter get_plugins)
   *
   * A list of loaded plugin names.
   *
   * Since: 1.0
   */
  properties [PROP_PLUGINS] =
    g_param_spec_boxed ("plugins", NULL, NULL,
                        G_TYPE_STRV,
                        (G_PARAM_READABLE |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  /**
   * ValentDevice:state: (getter get_state)
   *
   * The state of the device.
   *
   * Since: 1.0
   */
  properties [PROP_STATE] =
    g_param_spec_flags ("state", NULL, NULL,
                        VALENT_TYPE_DEVICE_STATE,
                        VALENT_DEVICE_STATE_NONE,
                        (G_PARAM_READABLE |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

/**
 * valent_device_new:
 * @id: (not nullable): a device ID
 *
 * Create a new device for @id.
 *
 * Returns: (transfer full) (nullable): a new #ValentDevice
 *
 * Since: 1.0
 */
ValentDevice *
valent_device_new (const char *id)
{
  g_return_val_if_fail (id != NULL && *id != '\0', NULL);

  return g_object_new (VALENT_TYPE_DEVICE,
                       "id", id,
                       NULL);
}

/*< private >
 * valent_device_new_full:
 * @identity: a KDE Connect identity packet
 * @context: (nullable): a #ValentContext
 *
 * Create a new device for @identity.
 *
 * Returns: (transfer full) (nullable): a new #ValentDevice
 */
ValentDevice *
valent_device_new_full (JsonNode      *identity,
                        ValentContext *context)
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
                      "id",      id,
                      "context", context,
                      NULL);
  valent_device_handle_identity (ret, identity);

  return ret;
}

static void
valent_device_send_packet_cb (ValentChannel *channel,
                              GAsyncResult  *result,
                              gpointer       user_data)
{
  g_autoptr (GTask) task = G_TASK (user_data);
  ValentDevice *device = g_task_get_source_object (task);
  g_autoptr (GError) error = NULL;

  g_assert (VALENT_IS_DEVICE (device));

  if (valent_channel_write_packet_finish (channel, result, &error))
    return g_task_return_boolean (task, TRUE);

  VALENT_NOTE ("%s: %s", device->name, error->message);
  g_task_return_error (task, g_steal_pointer (&error));

  valent_object_lock (VALENT_OBJECT (device));
  if (device->channel == channel)
    valent_device_set_channel (device, NULL);
  valent_object_unlock (VALENT_OBJECT (device));
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
  gboolean was_connected;
  gboolean is_connected;

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
  if ((was_connected = (device->channel != NULL)))
    {
      valent_channel_close_async (device->channel, NULL, NULL, NULL);
      g_clear_object (&device->channel);
    }

  /* If there's a new channel, handle the peer identity and queue the first
   * read operation before notifying of the state change. */
  if ((is_connected = g_set_object (&device->channel, channel)))
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

  /* If the state changed, update the plugins and notify */
  if (is_connected == was_connected)
    return;

  valent_device_update_plugins (device);
  g_object_notify_by_pspec (G_OBJECT (device), properties [PROP_STATE]);
}

/**
 * valent_device_get_context: (get-property context)
 * @device: a #ValentDevice
 *
 * Get the data context.
 *
 * Returns: (transfer full): a #ValentContext
 *
 * Since: 1.0
 */
ValentContext *
valent_device_get_context (ValentDevice *device)
{
  g_return_val_if_fail (VALENT_IS_DEVICE (device), NULL);

  return device->context;
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
 * valent_device_set_paired:
 * @device: a #ValentDevice
 * @paired: %TRUE if paired, %FALSE if unpaired
 *
 * Set the paired state of the device.
 *
 * NOTE: since valent_device_update_plugins() will be called as a side effect,
 * this must be called after valent_device_send_pair().
 */
void
valent_device_set_paired (ValentDevice *device,
                          gboolean      paired)
{
  g_assert (VALENT_IS_DEVICE (device));

  valent_object_lock (VALENT_OBJECT (device));

  /* If nothing's changed, only reset pending pair timeouts */
  if (device->paired == paired)
    {
      valent_device_reset_pair (device);
      valent_object_unlock (VALENT_OBJECT (device));
      return;
    }

  /* FIXME: If we're connected store/clear connection data */
  if (paired && device->channel != NULL)
    valent_channel_store_data (device->channel, device->context);
  else if (!paired)
    valent_context_clear (device->context);

  device->paired = paired;
  g_settings_set_boolean (device->settings, "paired", device->paired);

  valent_object_unlock (VALENT_OBJECT (device));

  /* Update plugins and notify */
  valent_device_update_plugins (device);
  valent_device_reset_pair (device);
}

/**
 * valent_device_get_plugins: (get-property plugins)
 * @device: a #ValentDevice
 *
 * Get a list of the loaded plugins.
 *
 * Returns: (transfer full): a list of loaded plugins
 *
 * Since: 1.0
 */
GStrv
valent_device_get_plugins (ValentDevice *device)
{
  g_autoptr (GStrvBuilder) builder = NULL;
  GHashTableIter iter;
  PeasPluginInfo *info;

  g_return_val_if_fail (VALENT_IS_DEVICE (device), NULL);

  builder = g_strv_builder_new ();
  g_hash_table_iter_init (&iter, device->plugins);

  while (g_hash_table_iter_next (&iter, (void **)&info, NULL))
    g_strv_builder_add (builder, peas_plugin_info_get_module_name (info));

  return g_strv_builder_end (builder);
}

/**
 * valent_device_get_state: (get-property state)
 * @device: a #ValentDevice
 *
 * Get the state of the device.
 *
 * Returns: #ValentDeviceState flags describing the state of the device
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
 * Pairing packets are handled by the device and the only packets accepted if
 * the device is unpaired. Any other packets received from an unpaired device
 * are ignored and a request to unpair will be sent to the remote device.
 *
 * Any other packets received from a paired device will be routed to each plugin
 * claiming to support it.
 */
void
valent_device_handle_packet (ValentDevice *device,
                             JsonNode     *packet)
{
  GPtrArray *handlers = NULL;
  const char *type;

  g_assert (VALENT_IS_DEVICE (device));
  g_assert (VALENT_IS_PACKET (packet));

  VALENT_JSON (packet, device->name);

  type = valent_packet_get_type (packet);

  if G_UNLIKELY (g_str_equal (type, "kdeconnect.pair"))
    {
      valent_device_handle_pair (device, packet);
    }
  else if G_UNLIKELY (!device->paired)
    {
      valent_device_send_pair (device, FALSE);
    }
  else if ((handlers = g_hash_table_lookup (device->handlers, type)) != NULL)
    {
      for (unsigned int i = 0, len = handlers->len; i < len; i++)
        {
          ValentDevicePlugin *handler = g_ptr_array_index (handlers, i);

          valent_device_plugin_handle_packet (handler, type, packet);
        }
    }
  else
    {
      VALENT_NOTE ("%s: Unsupported packet \"%s\"", device->name, type);
    }
}

/*< private >
 * valent_device_reload_plugins:
 * @device: a #ValentDevice
 *
 * Reload all plugins.
 *
 * Check each available plugin and load or unload them if the required
 * capabilities have changed.
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

/*< private >
 * valent_device_update_plugins:
 * @device: a #ValentDevice
 *
 * Update all plugins.
 *
 * Call [method@Valent.DevicePlugin.update_state] on each enabled plugin.
 */
static void
valent_device_update_plugins (ValentDevice *device)
{
  ValentDeviceState state = VALENT_DEVICE_STATE_NONE;
  GHashTableIter iter;
  ValentPlugin *plugin;

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

/*< private >
 * valent_device_supports_plugin:
 * @device: a #ValentDevice
 * @info: a #PeasPluginInfo
 *
 * Check if @device supports the plugin described by @info.
 *
 * Returns: %TRUE if supported, or %FALSE if not
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
  in_str = peas_plugin_info_get_external_data (info, "DevicePluginIncoming");
  out_str = peas_plugin_info_get_external_data (info, "DevicePluginOutgoing");

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

