// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-device"

#include "config.h"

#include <inttypes.h>
#include <math.h>

#include <glib/gi18n-lib.h>
#include <gio/gio.h>
#include <libvalent-core.h>

#include "../core/valent-component-private.h"
#include "valent-certificate.h"
#include "valent-channel.h"
#include "valent-device-common.h"
#include "valent-device-enums.h"
#include "valent-device-plugin.h"
#include "valent-packet.h"

#include "valent-device.h"
#include "valent-device-private.h"

#define DEVICE_TYPE_DESKTOP  "desktop"
#define DEVICE_TYPE_LAPTOP   "laptop"
#define DEVICE_TYPE_PHONE    "phone"
#define DEVICE_TYPE_TABLET   "tablet"
#define DEVICE_TYPE_TV       "tv"

#define PAIR_REQUEST_ID           "pair-request"
#define PAIR_REQUEST_TIMEOUT      (30)
#define PAIR_CLOCK_SKEW_THRESHOLD (1800)


/**
 * ValentDevice:
 *
 * A class representing a remote device, such as a smartphone or desktop.
 *
 * Device functionality is limited to pairing and sending packets, while other
 * functionality is delegated to [class@Valent.DevicePlugin] extensions.
 *
 * `ValentDevice` implements the [iface@Gio.ActionGroup] interface, acting as an
 * aggregate action group for plugins. Plugin actions are automatically included
 * in the device action group with the plugin module name as a prefix
 * (eg. `share.files`).
 *
 * Since: 1.0
 */

struct _ValentDevice
{
  ValentObject     parent_instance;

  ValentContext   *context;

  /* Properties */
  char            *icon_name;
  char            *id;
  char            *name;
  char            *type;
  char           **incoming_capabilities;
  char           **outgoing_capabilities;
  int64_t          protocol_version;

  /* State */
  GListModel      *channels;
  ValentChannel   *channel;
  gboolean         paired;
  unsigned int     incoming_pair;
  unsigned int     outgoing_pair;
  int64_t          pair_timestamp;

  /* Plugins */
  PeasEngine      *engine;
  GHashTable      *plugins;
  GHashTable      *handlers;
  GHashTable      *actions;
  GMenu           *menu;
};

static void   g_action_group_iface_init (GActionGroupInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (ValentDevice, valent_device, VALENT_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_ACTION_GROUP, g_action_group_iface_init))

typedef enum {
  PROP_CONTEXT = 1,
  PROP_ICON_NAME,
  PROP_ID,
  PROP_NAME,
  PROP_STATE,
} ValentDeviceProperty;

static GParamSpec *properties[PROP_STATE + 1] = { NULL, };


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
  g_autofree char *urn = NULL;
  const char *incoming = NULL;

  g_assert (VALENT_IS_DEVICE (device));
  g_assert (plugin != NULL);

  plugin->extension = peas_engine_create_extension (device->engine,
                                                    plugin->info,
                                                    VALENT_TYPE_DEVICE_PLUGIN,
                                                    "context",     plugin->context,
                                                    "parent",      plugin->parent,
                                                    NULL);
  g_return_if_fail (G_IS_OBJECT (plugin->extension));

  /* Register packet handlers
   */
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

  /* Register plugin actions
   */
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

  /* Bootstrap the newly instantiated plugin
   */
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

  /* Unregister actions
   */
  g_signal_handlers_disconnect_by_data (plugin->extension, plugin);
  actions = g_action_group_list_actions (G_ACTION_GROUP (plugin->extension));
  for (size_t i = 0; actions[i]; i++)
    {
      on_plugin_action_removed (G_ACTION_GROUP (plugin->extension),
                                actions[i],
                                plugin);
    }

  /* Unregister packet handlers
   */
  incoming = peas_plugin_info_get_external_data (plugin->info,
                                                 "DevicePluginIncoming");
  if (incoming != NULL)
    {
      g_auto (GStrv) capabilities = NULL;

      capabilities = g_strsplit (incoming, ";", -1);
      for (size_t i = 0; capabilities[i] != NULL; i++)
        {
          GPtrArray *handlers = NULL;

          handlers = g_hash_table_lookup (device->handlers, capabilities[i]);
          if (handlers == NULL)
            continue;

          g_ptr_array_remove (handlers, plugin->extension);
          if (handlers->len == 0)
            g_hash_table_remove (device->handlers, capabilities[i]);
        }
    }

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

  /* Plugins that don't handle packets aren't dependent on capabilities
   */
  in_str = peas_plugin_info_get_external_data (info, "DevicePluginIncoming");
  out_str = peas_plugin_info_get_external_data (info, "DevicePluginOutgoing");
  if (in_str == NULL && out_str == NULL)
    return TRUE;

  /* If capabilities are ready, check if the plugin outgoing matches the
   * incoming device or vice-versa.
   */
  device_incoming = (const char **)device->incoming_capabilities;
  device_outgoing = (const char **)device->outgoing_capabilities;
  if (device_incoming == NULL || device_outgoing == NULL)
    return FALSE;

  if (out_str != NULL)
    {
      g_auto (GStrv) plugin_outgoing = NULL;

      plugin_outgoing = g_strsplit (out_str, ";", -1);
      for (size_t i = 0; plugin_outgoing[i] != NULL; i++)
        {
          if (g_strv_contains (device_incoming, plugin_outgoing[i]))
            return TRUE;
        }
    }

  if (in_str != NULL)
    {
      g_auto (GStrv) plugin_incoming = NULL;

      plugin_incoming = g_strsplit (in_str, ";", -1);
      for (size_t i = 0; plugin_incoming[i] != NULL; i++)
        {
          if (g_strv_contains (device_outgoing, plugin_incoming[i]))
            return TRUE;
        }
    }

  return FALSE;
}

static void
on_load_plugin (PeasEngine     *engine,
                PeasPluginInfo *plugin_info,
                ValentDevice   *self)
{
  ValentPlugin *plugin;

  g_assert (PEAS_IS_ENGINE (engine));
  g_assert (plugin_info != NULL);
  g_assert (VALENT_IS_DEVICE (self));

  if (!valent_device_supports_plugin (self, plugin_info))
    return;

  VALENT_NOTE ("%s: %s",
               self->name,
               peas_plugin_info_get_module_name (plugin_info));

  plugin = valent_plugin_new (self, self->context, plugin_info,
                              G_CALLBACK (on_plugin_enabled_changed));
  g_hash_table_insert (self->plugins, plugin_info, plugin);

  if (valent_plugin_get_enabled (plugin))
    valent_device_enable_plugin (self, plugin);
}

static void
on_unload_plugin (PeasEngine     *engine,
                  PeasPluginInfo *plugin_info,
                  ValentDevice   *device)
{
  g_assert (PEAS_IS_ENGINE (engine));
  g_assert (plugin_info != NULL);
  g_assert (VALENT_IS_DEVICE (device));

  if (g_hash_table_remove (device->plugins, plugin_info))
    {
      VALENT_NOTE ("%s: %s",
                   device->name,
                   peas_plugin_info_get_module_name (plugin_info));
    }
}

static void
valent_device_reload_plugins (ValentDevice *self)
{
  unsigned int n_plugins = 0;

  g_assert (VALENT_IS_DEVICE (self));

  n_plugins = g_list_model_get_n_items (G_LIST_MODEL (self->engine));
  for (unsigned int i = 0; i < n_plugins; i++)
    {
      g_autoptr (PeasPluginInfo) plugin_info = NULL;

      plugin_info = g_list_model_get_item (G_LIST_MODEL (self->engine), i);
      if (!g_hash_table_contains (self->plugins, plugin_info))
        on_load_plugin (self->engine, plugin_info, self);
      else if (!valent_device_supports_plugin (self, plugin_info))
        on_unload_plugin (self->engine, plugin_info, self);
    }
}

static void
valent_device_update_plugins (ValentDevice *self)
{
  ValentDeviceState state = VALENT_DEVICE_STATE_NONE;
  GHashTableIter iter;
  ValentPlugin *plugin;

  g_assert (VALENT_IS_DEVICE (self));

  state = valent_device_get_state (self);

  g_hash_table_iter_init (&iter, self->plugins);
  while (g_hash_table_iter_next (&iter, NULL, (void **)&plugin))
    {
      if (plugin->extension == NULL)
        continue;

      valent_device_plugin_update_state (VALENT_DEVICE_PLUGIN (plugin->extension),
                                         state);
    }
}

/*
 * Private pairing methods
 */
static void
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
  device->pair_timestamp = 0;
}

static gboolean
valent_device_cancel_pair (gpointer object)
{
  ValentDevice *self = VALENT_DEVICE (object);

  g_assert (VALENT_IS_DEVICE (self));

  if (self->incoming_pair != 0 || self->outgoing_pair != 0)
    {
      valent_device_reset_pair (self);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_STATE]);
    }

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
      valent_object_destroy (VALENT_OBJECT (channel));
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

  if (device->channel == NULL)
    return;

  valent_packet_init (&builder, "kdeconnect.pair");
  json_builder_set_member_name (builder, "pair");
  json_builder_add_boolean_value (builder, pair);
  if (device->protocol_version >= VALENT_NETWORK_PROTOCOL_V8 &&
      device->pair_timestamp == 0 && pair)
    {
      device->pair_timestamp = (int64_t)floor (valent_timestamp_ms () / 1000);
      json_builder_set_member_name (builder, "timestamp");
      json_builder_add_int_value (builder, device->pair_timestamp);
    }
  packet = valent_packet_end (&builder);

  cancellable = valent_object_ref_cancellable (VALENT_OBJECT (device));
  valent_channel_write_packet (device->channel,
                               packet,
                               cancellable,
                               (GAsyncReadyCallback)send_pair_cb,
                               g_object_ref (device));
}

static void
valent_device_set_paired (ValentDevice *self,
                          gboolean      paired)
{
  g_autoptr (GFile) certificate_file = NULL;
  g_autoptr (GError) error = NULL;

  g_assert (VALENT_IS_DEVICE (self));

  if (self->paired == paired)
    {
      valent_device_cancel_pair (self);
      return;
    }

  /* Store the certificate in the configuration directory if paired,
   * otherwise delete the certificate and clear the data context.
   *
   * TODO: KDE Connect uses certificate pinning, but there is no shared resource
   *       mediating between a ValentDevice and a ValentChannelService.
   */
  certificate_file = valent_context_get_config_file (self->context,
                                                     "certificate.pem");
  if (paired)
    {
      g_autoptr (GTlsCertificate) certificate = NULL;
      g_autofree char *certificate_pem = NULL;

      if (self->channel != NULL)
        {
          g_object_get (self->channel,
                        "peer-certificate", &certificate,
                        NULL);
          g_object_get (certificate,
                        "certificate-pem", &certificate_pem,
                        NULL);
          g_file_replace_contents (certificate_file,
                                   certificate_pem,
                                   strlen (certificate_pem),
                                   NULL,  /* etag */
                                   FALSE, /* make_backup */
                                   (G_FILE_CREATE_PRIVATE |
                                    G_FILE_CREATE_REPLACE_DESTINATION),
                                   NULL,  /* etag (out) */
                                   NULL,
                                   &error);
          if (error != NULL)
            {
              g_warning ("%s(): failed to write \"%s\": %s",
                         G_STRFUNC,
                         g_file_peek_path (certificate_file),
                         error->message);
            }
        }
    }
  else
    {
      if (!g_file_delete (certificate_file, NULL, &error) &&
          !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
        {
          g_warning ("%s(): failed to delete \"%s\": %s",
                     G_STRFUNC,
                     g_file_peek_path (certificate_file),
                     error->message);
        }

      valent_context_clear (self->context);
    }

  self->paired = paired;

  valent_device_reset_pair (self);
  valent_device_update_plugins (self);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_STATE]);
}

static void
valent_device_notify_pair_requested (ValentDevice *device)
{
  GApplication *application = NULL;
  g_autofree char *notification_id = NULL;
  g_autoptr (GNotification) notification = NULL;
  g_autoptr (GIcon) icon = NULL;
  g_autofree char *title = NULL;
  g_autofree char *verification_key = NULL;

  g_assert (VALENT_IS_DEVICE (device));

  application = g_application_get_default ();
  if (application == NULL)
    return;

  title = g_strdup_printf (_("Pairing request from “%s”"), device->name);
  verification_key = valent_device_get_verification_key (device);
  icon = g_themed_icon_new (APPLICATION_ID);

  g_return_if_fail (verification_key != NULL);

  notification = g_notification_new (title);
  g_notification_set_body (notification, verification_key);
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

  /* Show the pairing notification and set a timeout for 30s
   */
  notification_id = g_strdup_printf ("%s::%s", device->id, PAIR_REQUEST_ID);
  g_application_send_notification (application,
                                   notification_id,
                                   notification);
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
      g_warning ("%s(): expected \"pair\" field holding a boolean from \"%s\"",
                 G_STRFUNC,
                 device->name);
      VALENT_EXIT;
    }

  if (pair)
    {
      if (device->outgoing_pair > 0)
        {
          g_debug ("Pairing accepted by \"%s\"", device->name);
          valent_device_set_paired (device, TRUE);
        }
      else if (device->incoming_pair > 0)
        {
          g_debug ("Ignoring concurrent pairing request from \"%s\"",
                   device->name);
        }
      else
        {
          g_debug ("Pairing requested by \"%s\"", device->name);

          if (device->protocol_version >= VALENT_NETWORK_PROTOCOL_V8)
            {
              int64_t timestamp = 0;
              int64_t localtime = 0;

              if (!valent_packet_get_int (packet, "timestamp", &timestamp))
                {
                  g_warning ("%s(): expected \"timestamp\" field holding an integer",
                             G_STRFUNC);
                  VALENT_EXIT;
                }

              localtime = (int64_t)floor (valent_timestamp_ms () / 1000);
              if (ABS (localtime - timestamp) > PAIR_CLOCK_SKEW_THRESHOLD)
                {
                  g_warning ("%s(): device clocks are out of sync", G_STRFUNC);
                  VALENT_EXIT;
                }

              device->pair_timestamp = timestamp;
            }

          if (device->paired)
            {
              /* In protocol v8, the timestamp field is mandatory for pair
               * requests; unsolicited pair accept packets have been handled
               * at this point, so the request can safely be auto-accepted.
               *
               * In protocol v7, pair request and accept packets are identical;
               * if both devices interpret unsolicited accept packets as
               * requests, they will get caught in an infinite loop.
               */
              if (device->pair_timestamp > 0)
                {
                  g_debug ("Accepting pair request from paired device \"%s\"",
                           device->name);
                  valent_device_send_pair (device, TRUE);
                  valent_device_set_paired (device, TRUE);
                  VALENT_EXIT;
                }
              else
                {
                  g_warning ("Pairing requested by paired device \"%s\"",
                             device->name);
                  valent_device_set_paired (device, FALSE);
                }
            }

          device->incoming_pair = g_timeout_add_seconds (PAIR_REQUEST_TIMEOUT,
                                                         valent_device_cancel_pair,
                                                         device);
          g_object_notify_by_pspec (G_OBJECT (device), properties [PROP_STATE]);
          valent_device_notify_pair_requested (device);
        }
    }
  else
    {
      g_debug ("Pairing rejected by \"%s\"", device->name);
      valent_device_set_paired (device, FALSE);
    }

  VALENT_EXIT;
}

/*
 * Private identity methods
 */
static char *
valent_device_sanitize_name (const char *name,
                             const char *fallback)
{
  static const uint8_t forbidden_chars[256] = {
    ['"'] = 1, ['\''] = 1,
    [','] = 1, ['.'] = 1,
    [';'] = 1, [':'] = 1,
    ['!'] = 1, ['?'] = 1,
    ['('] = 1, [')'] = 1,
    ['['] = 1, [']'] = 1,
    ['<'] = 1, ['>'] = 1,
  };
  g_autoptr (GString) ret = NULL;

  g_assert (name != NULL && *name != '\0');

  ret = g_string_new (NULL);
  for (const char *p = name; *p != '\0'; p = g_utf8_next_char (p))
    {
      gunichar ch = g_utf8_get_char (p);
      if (ch > 127 || !forbidden_chars[(uint8_t)ch])
        g_string_append_unichar (ret, ch);

      if (ret->len == 32)
        break;
    }

  if (ret->len == 0)
    {
      g_warning ("%s(): device name \"%s\" could not be sanitized",
                 G_STRFUNC,
                 name);
      return g_strdup (fallback);
    }

  return g_string_free_and_steal (g_steal_pointer (&ret));
}

static void
valent_device_handle_identity (ValentDevice *device,
                               JsonNode     *packet)
{
  const char *device_id;
  const char *device_name;
  const char *device_type;
  g_autofree char *sanitized_name = NULL;

  VALENT_ENTRY;

  g_assert (VALENT_IS_DEVICE (device));
  g_assert (VALENT_IS_PACKET (packet));

  /* The ID must match the construct-time value.
   */
  if (!valent_packet_get_string (packet, "deviceId", &device_id) ||
      !g_str_equal (device->id, device_id))
    {
      g_critical ("%s(): expected \"deviceId\" field holding \"%s\"",
                  G_STRFUNC,
                  device->id);
      VALENT_EXIT;
    }

  /* If the device name is invalid, try removing the offended characters,
   * falling back to the device ID.
   */
  if (!valent_packet_get_string (packet, "deviceName", &device_name))
    {
      g_critical ("%s(): expected \"deviceName\" field holding a string",
                  G_STRFUNC);
      VALENT_EXIT;
    }

  sanitized_name = valent_device_sanitize_name (device_name, device_id);
  if (g_set_str (&device->name, sanitized_name))
    g_object_notify_by_pspec (G_OBJECT (device), properties [PROP_NAME]);

  /* The device type is only used to generate an icon name.
   */
  if (!valent_packet_get_string (packet, "deviceType", &device_type))
    {
      g_warning ("%s(): expected \"deviceType\" field holding a string",
                  G_STRFUNC);
      device_type = DEVICE_TYPE_DESKTOP;
    }

  if (g_set_str (&device->type, device_type))
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

      if (g_set_str (&device->icon_name, device_icon))
        g_object_notify_by_pspec (G_OBJECT (device), properties [PROP_ICON_NAME]);
    }

  /* In practice these are static, but in principle could change with the
   * channel (e.g. TCP and Bluetooth).
   */
  g_clear_pointer (&device->incoming_capabilities, g_strfreev);
  device->incoming_capabilities =
    valent_packet_dup_strv (packet, "incomingCapabilities");

  g_clear_pointer (&device->outgoing_capabilities, g_strfreev);
  device->outgoing_capabilities =
    valent_packet_dup_strv (packet, "outgoingCapabilities");

  /* It's not clear if this is only a required field for TLS connections,
   * or if it applies to Bluetooth as well.
   */
  if (!valent_packet_get_int (packet, "protocolVersion", &device->protocol_version))
    device->protocol_version = VALENT_NETWORK_PROTOCOL_V8;

  /* Recheck plugins against current capabilities
   */
  valent_device_reload_plugins (device);

  VALENT_EXIT;
}

static void
valent_device_handle_packet (ValentDevice *self,
                             JsonNode     *packet)
{
  GPtrArray *handlers = NULL;
  const char *type;

  g_assert (VALENT_IS_DEVICE (self));
  g_assert (VALENT_IS_PACKET (packet));

  VALENT_JSON (packet, self->name);

  type = valent_packet_get_type (packet);
  if G_UNLIKELY (g_str_equal (type, "kdeconnect.pair"))
    {
      valent_device_handle_pair (self, packet);
      return;
    }

  if G_UNLIKELY (!self->paired)
    {
      g_debug ("%s(): unexpected \"%s\" packet from unpaired device %s",
               G_STRFUNC, type, self->name);
      valent_device_send_pair (self, FALSE);
      return;
    }

  handlers = g_hash_table_lookup (self->handlers, type);
  if G_UNLIKELY (handlers == NULL)
    {
      g_debug ("%s(): unsupported \"%s\" packet from %s",
               G_STRFUNC, type, self->name);
      return;
    }

  for (unsigned int i = 0, len = handlers->len; i < len; i++)
    {
      ValentDevicePlugin *handler = g_ptr_array_index (handlers, i);

      valent_device_plugin_handle_packet (handler, type, packet);
    }
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

  if (device->incoming_pair > 0)
    {
      g_debug ("Accepting pair request from \"%s\"", device->name);

      valent_device_send_pair (device, TRUE);
      valent_device_set_paired (device, TRUE);
    }
  else if (device->outgoing_pair > 0)
    {
      g_warning ("Pairing request with \"%s\" already in progress",
                 device->name);
    }
  else if (device->paired)
    {
      g_warning ("Ignoring attempt to pair with paired device \"%s\"",
                 device->name);
    }
  else
    {
      g_debug ("Sending pair request to \"%s\"", device->name);

      valent_device_send_pair (device, TRUE);
      device->outgoing_pair = g_timeout_add_seconds (PAIR_REQUEST_TIMEOUT,
                                                     valent_device_cancel_pair,
                                                     device);
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
 * ValentObject
 */
static void
valent_device_destroy (ValentObject *object)
{
  ValentDevice *self = VALENT_DEVICE (object);
  unsigned int n_items = 0;

  valent_device_reset_pair (self);

  /* Destroy all connections associated with this device,
   * ensuring a state change is propagated to plugins
   */
  n_items = g_list_model_get_n_items (self->channels);
  for (unsigned int i = n_items; i--;)
    {
      g_autoptr (ValentObject) item = NULL;

      item = g_list_model_get_item (self->channels, i);
      valent_object_destroy (item);
    }

  g_signal_handlers_disconnect_by_data (self->engine, self);
  g_hash_table_remove_all (self->plugins);
  g_hash_table_remove_all (self->actions);
  g_hash_table_remove_all (self->handlers);

  VALENT_OBJECT_CLASS (valent_device_parent_class)->destroy (object);
}

/*
 * GObject
 */
static void
valent_device_constructed (GObject *object)
{
  ValentDevice *self = VALENT_DEVICE (object);
  g_autoptr (GFile) certificate_file = NULL;
  unsigned int n_plugins = 0;

  G_OBJECT_CLASS (valent_device_parent_class)->constructed (object);

  /* We must at least have a device ID */
  g_assert (self->id != NULL);

  if (self->context == NULL)
    {
      g_autoptr (ValentContext) context = NULL;
      ValentObject *parent = NULL;

      parent = valent_object_get_parent (VALENT_OBJECT (self));
      if (parent != NULL)
        g_object_get (parent, "context", &context, NULL);

      self->context = valent_context_new (context, "device", self->id);
    }

  certificate_file = valent_context_get_config_file (self->context,
                                                     "certificate.pem");
  self->paired = g_file_query_exists (certificate_file, NULL);

  self->engine = valent_get_plugin_engine ();
  g_signal_connect_object (self->engine,
                           "load-plugin",
                           G_CALLBACK (on_load_plugin),
                           self,
                           G_CONNECT_AFTER);

  g_signal_connect_object (self->engine,
                           "unload-plugin",
                           G_CALLBACK (on_unload_plugin),
                           self,
                           G_CONNECT_DEFAULT);

  n_plugins = g_list_model_get_n_items (G_LIST_MODEL (self->engine));
  for (unsigned int i = 0; i < n_plugins; i++)
    {
      g_autoptr (PeasPluginInfo) info = NULL;

      info = g_list_model_get_item (G_LIST_MODEL (self->engine), i);
      if (peas_plugin_info_is_loaded (info))
        on_load_plugin (self->engine, info, self);
    }
}

static void
valent_device_finalize (GObject *object)
{
  ValentDevice *self = VALENT_DEVICE (object);

  g_clear_object (&self->context);

  /* Properties */
  g_clear_pointer (&self->icon_name, g_free);
  g_clear_pointer (&self->id, g_free);
  g_clear_pointer (&self->name, g_free);
  g_clear_pointer (&self->type, g_free);
  g_clear_pointer (&self->incoming_capabilities, g_strfreev);
  g_clear_pointer (&self->outgoing_capabilities, g_strfreev);

  /* State */
  g_clear_object (&self->channels);
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

  switch ((ValentDeviceProperty)prop_id)
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

  switch ((ValentDeviceProperty)prop_id)
    {
    case PROP_CONTEXT:
      self->context = g_value_dup_object (value);
      break;

    case PROP_ID:
      self->id = g_value_dup_string (value);
      break;

    case PROP_ICON_NAME:
    case PROP_NAME:
    case PROP_STATE:
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_device_init (ValentDevice *self)
{
  GSimpleAction *action = NULL;

  self->channels = G_LIST_MODEL (g_list_store_new (VALENT_TYPE_CHANNEL));
  self->plugins = g_hash_table_new_full (NULL, NULL, NULL, valent_plugin_free);
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
  g_signal_connect_object (action,
                           "activate",
                           G_CALLBACK (pair_action),
                           self,
                           G_CONNECT_DEFAULT);
  g_hash_table_replace (self->actions,
                        g_strdup ("pair"),
                        g_steal_pointer (&action));

  action = g_simple_action_new ("unpair", NULL);
  g_signal_connect_object (action,
                           "activate",
                           G_CALLBACK (unpair_action),
                           self,
                           G_CONNECT_DEFAULT);
  g_hash_table_replace (self->actions,
                        g_strdup ("unpair"),
                        g_steal_pointer (&action));
}

static void
valent_device_class_init (ValentDeviceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentObjectClass *vobject_class = VALENT_OBJECT_CLASS (klass);

  object_class->constructed = valent_device_constructed;
  object_class->finalize = valent_device_finalize;
  object_class->get_property = valent_device_get_property;
  object_class->set_property = valent_device_set_property;

  vobject_class->destroy = valent_device_destroy;

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

  g_object_class_install_properties (object_class, G_N_ELEMENTS (properties), properties);
}

/*< private >
 * valent_device_new_full:
 * @parent: (nullable): a `ValentObject`
 * @identity: a KDE Connect identity packet
 *
 * Create a new device for @identity.
 *
 * Returns: (transfer full) (nullable): a new `ValentDevice`
 */
ValentDevice *
valent_device_new_full (ValentObject *parent,
                        JsonNode     *identity)
{
  ValentDevice *ret;
  const char *id;

  g_return_val_if_fail (parent == NULL || VALENT_IS_OBJECT (parent), NULL);
  g_return_val_if_fail (VALENT_IS_PACKET (identity), NULL);

  if (!valent_packet_get_string (identity, "deviceId", &id))
    {
      g_critical ("%s(): missing \"deviceId\" field", G_STRFUNC);
      return NULL;
    }

  if (!valent_device_validate_id (id))
    {
      g_critical ("%s(): invalid device ID \"%s\"", G_STRFUNC, id);
      return NULL;
    }

  ret = g_object_new (VALENT_TYPE_DEVICE,
                      "id",     id,
                      "parent", parent,
                      NULL);
  valent_device_handle_identity (ret, identity);

  return ret;
}

static void
valent_device_send_packet_cb (ValentChannel *channel,
                              GAsyncResult  *result,
                              gpointer       user_data)
{
  g_autoptr (GTask) task = G_TASK (g_steal_pointer (&user_data));
  ValentDevice *self = g_task_get_source_object (task);
  GCancellable *cancellable = g_task_get_cancellable (task);
  JsonNode *packet = g_task_get_task_data (task);
  g_autoptr (GError) error = NULL;

  if (valent_channel_write_packet_finish (channel, result, &error))
    {
      VALENT_JSON (packet, self->name);
      g_task_return_boolean (task, TRUE);
      return;
    }

  /* Unless the operation was cancelled, destroy the defunct
   * channel and retry if possible
   */
  if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
      valent_object_destroy (VALENT_OBJECT (channel));
      if (self->channel != NULL)
        {
          valent_channel_write_packet (self->channel,
                                       packet,
                                       cancellable,
                                       (GAsyncReadyCallback)valent_device_send_packet_cb,
                                       g_object_ref (task));
          return;
        }
    }

  g_task_return_error (task, g_steal_pointer (&error));
}

/**
 * valent_device_send_packet:
 * @device: a `ValentDevice`
 * @packet: a KDE Connect packet
 * @cancellable: (nullable): a `GCancellable`
 * @callback: (scope async): a `GAsyncReadyCallback`
 * @user_data: user supplied data
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

  if G_UNLIKELY (device->channel == NULL)
    {
      g_task_report_new_error (device,
                               callback,
                               user_data,
                               valent_device_send_packet,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_CONNECTED,
                               "%s is disconnected", device->name);
      return;
    }

  if G_UNLIKELY (!device->paired)
    {
      g_task_report_new_error (device,
                               callback,
                               user_data,
                               valent_device_send_packet,
                               G_IO_ERROR,
                               G_IO_ERROR_PERMISSION_DENIED,
                               "%s is unpaired", device->name);
      return;
    }

  task = g_task_new (device, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_device_send_packet);
  g_task_set_task_data (task,
                        json_node_ref (packet),
                        (GDestroyNotify)json_node_unref);
  valent_channel_write_packet (device->channel,
                               packet,
                               cancellable,
                               (GAsyncReadyCallback)valent_device_send_packet_cb,
                               g_steal_pointer (&task));
}

/**
 * valent_device_send_packet_finish:
 * @device: a `ValentDevice`
 * @result: a `GAsyncResult`
 * @error: (nullable): a `GError`
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

static void
read_packet_cb (ValentChannel *channel,
                GAsyncResult  *result,
                ValentDevice  *device)
{
  g_autoptr (GError) error = NULL;
  g_autoptr (JsonNode) packet = NULL;

  g_assert (VALENT_IS_CHANNEL (channel));
  g_assert (VALENT_IS_DEVICE (device));

  /* If successful, queue another read before handling the packet. Otherwise
   * drop our reference if it's still the active channel.
   */
  packet = valent_channel_read_packet_finish (channel, result, &error);
  if (packet != NULL)
    {
      valent_channel_read_packet (channel,
                                  g_task_get_cancellable (G_TASK (result)),
                                  (GAsyncReadyCallback)read_packet_cb,
                                  g_object_ref (device));
      valent_device_handle_packet (device, packet);
    }
  else
    {
      VALENT_NOTE ("%s: %s", device->name, error->message);
      valent_object_destroy (VALENT_OBJECT (channel));
    }

  g_object_unref (device);
}

/**
 * valent_device_get_channels: (get-property channels)
 * @device: a `ValentDevice`
 *
 * Get the active channels.
 *
 * Returns: (transfer none): a `GListModel`
 *
 * Since: 1.0
 */
GListModel *
valent_device_get_channels (ValentDevice *device)
{
  g_return_val_if_fail (VALENT_IS_DEVICE (device), NULL);

  return device->channels;
}

static void
on_channel_destroy (ValentObject *object,
                    ValentDevice *self)
{
  ValentChannel *channel = VALENT_CHANNEL (object);
  unsigned int position = 0;

  if (g_list_store_find (G_LIST_STORE (self->channels), object, &position))
    {
      g_list_store_remove (G_LIST_STORE (self->channels), position);

      /* Drop the extra reference if this was the highest priority channel
       */
      if (self->channel == channel)
        {
          g_warn_if_fail (position == 0);
          g_clear_object (&self->channel);
        }

      /* Notify of the state change if this was the last channel,
       * or ensure a reference to the channel with the highest priority.
       */
      if (g_list_model_get_n_items (self->channels) == 0)
        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_STATE]);
      else if (self->channel == NULL)
        self->channel = g_list_model_get_item (self->channels, 0);
    }
}

static inline int
channel_sort_func (gconstpointer a,
                  gconstpointer b,
                  gpointer      user_data)
{
  if (g_str_equal (G_OBJECT_TYPE_NAME (a), G_OBJECT_TYPE_NAME (b)))
    return 0;

  return g_str_equal (G_OBJECT_TYPE_NAME (a), "ValentLanChannel") ? -1 : 1;
}

/**
 * valent_device_add_channel:
 * @device: a `ValentDevice`
 * @channel: a `ValentChannel`
 *
 * Add @channel to the list of active channels.
 *
 * Since: 1.0
 */
void
valent_device_add_channel (ValentDevice  *device,
                           ValentChannel *channel)
{
  JsonNode *peer_identity;
  g_autoptr (GCancellable) cancellable = NULL;
  unsigned int n_channels = 0;
  unsigned int position = 0;

  g_return_if_fail (VALENT_IS_DEVICE (device));
  g_return_if_fail (VALENT_IS_CHANNEL (channel));

  g_signal_connect_object (channel,
                           "destroy",
                           G_CALLBACK (on_channel_destroy),
                           device,
                           G_CONNECT_DEFAULT);

  n_channels = g_list_model_get_n_items (device->channels);
  position = g_list_store_insert_sorted (G_LIST_STORE (device->channels),
                                         channel,
                                         channel_sort_func,
                                         NULL);

  /* Handle the peer identity and start reading the first packet
   * before notifying of the state change
   */
  peer_identity = valent_channel_get_peer_identity (channel);
  valent_device_handle_identity (device, peer_identity);

  cancellable = valent_object_ref_cancellable (VALENT_OBJECT (device));
  valent_channel_read_packet (channel,
                              cancellable,
                              (GAsyncReadyCallback)read_packet_cb,
                              g_object_ref (device));

  /* Hold a reference to the channel if it has the highest priority,
   * and notify of the state change if it's the first channel
   */
  if (position == 0)
    g_set_object (&device->channel, channel);

  if (n_channels == 0)
    {
      valent_device_update_plugins (device);
      g_object_notify_by_pspec (G_OBJECT (device), properties[PROP_STATE]);
    }
}

/**
 * valent_device_get_context: (get-property context)
 * @device: a `ValentDevice`
 *
 * Get the data context.
 *
 * Returns: (transfer full): a `ValentContext`
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
 * @device: a `ValentDevice`
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
 * @device: a `ValentDevice`
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
 * @device: a `ValentDevice`
 *
 * Get the [class@Gio.MenuModel] of the device.
 *
 * Plugins may add items and submenus to this when they want to expose actions
 * with presentation details like a label or icon.
 *
 * Returns: (transfer none): a `GMenuModel`
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
 * @device: a `ValentDevice`
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
 * valent_device_get_state: (get-property state)
 * @device: a `ValentDevice`
 *
 * Get the state of the device.
 *
 * Returns: `ValentDeviceState` flags describing the state of the device
 *
 * Since: 1.0
 */
ValentDeviceState
valent_device_get_state (ValentDevice *device)
{
  ValentDeviceState state = VALENT_DEVICE_STATE_NONE;

  g_return_val_if_fail (VALENT_IS_DEVICE (device), state);

  if (device->channel != NULL)
    state |= VALENT_DEVICE_STATE_CONNECTED;

  if (device->paired)
    state |= VALENT_DEVICE_STATE_PAIRED;

  if (device->incoming_pair > 0)
    state |= VALENT_DEVICE_STATE_PAIR_INCOMING;

  if (device->outgoing_pair > 0)
    state |= VALENT_DEVICE_STATE_PAIR_OUTGOING;

  return state;
}

/**
 * valent_device_get_verification_key:
 * @device: a `ValentDevice`
 *
 * Get a verification key for the device connection.
 *
 * Returns: (nullable) (transfer full): a verification key
 *
 * Since: 1.0
 */
char *
valent_device_get_verification_key (ValentDevice *device)
{
  char *verification_key = NULL;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_DEVICE (device), NULL);

  if (device->channel != NULL)
    {
      g_autoptr (GChecksum) checksum = NULL;
      g_autoptr (GTlsCertificate) cert = NULL;
      g_autoptr (GTlsCertificate) peer_cert = NULL;
      GByteArray *pubkey;
      GByteArray *peer_pubkey;
      size_t cmplen;

      cert = valent_channel_ref_certificate (device->channel);
      peer_cert = valent_channel_ref_peer_certificate (device->channel);
      g_return_val_if_fail (cert != NULL || peer_cert != NULL, NULL);

      pubkey = valent_certificate_get_public_key (cert);
      peer_pubkey = valent_certificate_get_public_key (peer_cert);
      g_return_val_if_fail (pubkey != NULL || peer_pubkey != NULL, NULL);

      checksum = g_checksum_new (G_CHECKSUM_SHA256);
      cmplen = MIN (pubkey->len, peer_pubkey->len);
      if (memcmp (pubkey->data, peer_pubkey->data, cmplen) > 0)
        {
          g_checksum_update (checksum, pubkey->data, pubkey->len);
          g_checksum_update (checksum, peer_pubkey->data, peer_pubkey->len);
        }
      else
        {
          g_checksum_update (checksum, peer_pubkey->data, peer_pubkey->len);
          g_checksum_update (checksum, pubkey->data, pubkey->len);
        }

      if (device->protocol_version >= VALENT_NETWORK_PROTOCOL_V8)
        {
          g_autofree char *timestamp_str = NULL;

          timestamp_str = g_strdup_printf ("%"PRId64, device->pair_timestamp);
          g_checksum_update (checksum, (const unsigned char *)timestamp_str, -1);
        }

      verification_key = g_ascii_strup (g_checksum_get_string (checksum), 8);
    }

  VALENT_RETURN (verification_key);
}

/**
 * valent_device_generate_id:
 *
 * Generate a new KDE Connect device ID.
 *
 * See [func@Valent.Device.validate_id] for a description of valid device ID.
 *
 * Returns: (transfer full): a new KDE Connect device ID
 *
 * Since: 1.0
 */
char *
valent_device_generate_id (void)
{
  static const char hex_digits[16 + 1] = "0123456789abcdef";
  char *id = g_new (char, 32 + 1);

  for (size_t i = 0; i < 32; i++)
    id[i] = hex_digits[g_random_int_range (0, 16)];
  id[32] = '\0';

  return g_steal_pointer (&id);
}

/**
 * valent_device_validate_id:
 * @id: (nullable): a KDE Connect device ID
 *
 * Validate a KDE Connect device ID.
 *
 * A compliant device ID matches the pattern `/^[a-zA-Z0-9]{32,38}$/`, being
 * alphanumeric with a length of 32-38 characters. However, for backwards
 * compatibility, implementations must accept device IDs that include hyphens
 * and underscores.
 *
 * This became a requirement in version 8 of the KDE Connect protocol.
 *
 * Returns: %TRUE if valid, or %FALSE
 *
 * Since: 1.0
 */
gboolean
valent_device_validate_id (const char *id)
{
  size_t len = 0;

  if G_UNLIKELY (id == NULL || *id == '\0')
    return FALSE;

  while (id[len] != '\0')
    {
      char c = id[len];

      if (!g_ascii_isalnum (c) && c != '_' && c != '-')
        return FALSE;

      if (++len > 38)
        return FALSE;
    }

  return len >= 32;
}

/**
 * valent_device_validate_name:
 * @name: (nullable): a KDE Connect device name
 *
 * Validate a KDE Connect device name.
 *
 * A compliant device name matches the pattern `/^[^"',;:.!?()\[\]<>]{1,32}$/`,
 * containing none of `"',;:.!?()[]<>` with a length of 1-32 characters, with
 * at least one non-whitespace character.
 *
 * This became a requirement in version 8 or the KDE Connect protocol.
 *
 * Returns: %TRUE if valid, or %FALSE
 *
 * Since: 1.0
 */
gboolean
valent_device_validate_name (const char *name)
{
  static const uint8_t forbidden_chars[256] = {
    ['"'] = 1, ['\''] = 1,
    [','] = 1, ['.'] = 1,
    [';'] = 1, [':'] = 1,
    ['!'] = 1, ['?'] = 1,
    ['('] = 1, [')'] = 1,
    ['['] = 1, [']'] = 1,
    ['<'] = 1, ['>'] = 1,
  };
  size_t len = 0;
  gboolean has_nonwhitespace = FALSE;

  if G_UNLIKELY (name == NULL || *name == '\0')
    return FALSE;

  for (const char *p = name; *p != '\0'; p = g_utf8_next_char (p))
    {
      gunichar ch = g_utf8_get_char (p);
      if (ch <= 127 && forbidden_chars[(uint8_t)ch])
        return FALSE;

      if (!has_nonwhitespace)
        has_nonwhitespace = !g_unichar_isspace (ch);

      if (++len > 32)
        return FALSE;
    }

  return has_nonwhitespace;
}

