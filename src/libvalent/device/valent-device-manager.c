// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-device-manager"

#include "config.h"

#include <gio/gio.h>
#include <libvalent-core.h>

#include "../core/valent-component-private.h"
#include "valent-certificate.h"
#include "valent-channel.h"
#include "valent-channel-service.h"
#include "valent-device.h"
#include "valent-device-impl.h"
#include "valent-device-manager.h"
#include "valent-device-private.h"
#include "valent-packet.h"

#define DEVICE_UNPAIRED_MAX (10)


/**
 * ValentDeviceManager:
 *
 * A class for discovering and managing devices.
 *
 * `ValentDeviceManager` manages the available [class@Valent.Device] objects,
 * connecting them when [signal@Valent.ChannelService::channel] is emitted by an
 * enabled implementation, exporting them on D-Bus and removing them when they
 * become unavailable.
 *
 * Since: 1.0
 */

struct _ValentDeviceManager
{
  ValentApplicationPlugin   parent_instance;

  GSettings                *settings;
  GCancellable             *cancellable;
  ValentContext            *context;

  GPtrArray                *devices;
  GHashTable               *plugins;
  ValentContext            *plugins_context;
  JsonNode                 *state;

  GDBusObjectManagerServer *dbus;
  GHashTable               *exports;
};

static void           valent_device_manager_add_device    (ValentDeviceManager *manager,
                                                           ValentDevice        *device);
static void           valent_device_manager_remove_device (ValentDeviceManager *manager,
                                                           ValentDevice        *device);
static ValentDevice * valent_device_manager_ensure_device (ValentDeviceManager *manager,
                                                           JsonNode            *identity);

static void   g_list_model_iface_init     (GListModelInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (ValentDeviceManager, valent_device_manager, VALENT_TYPE_APPLICATION_PLUGIN,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, g_list_model_iface_init))

static ValentDeviceManager *default_manager = NULL;


static inline void
_valent_object_deref (gpointer data)
{
  g_assert (VALENT_IS_OBJECT (data));

  valent_object_destroy (VALENT_OBJECT (data));
  g_object_unref (data);
}

/*
 * GListModel
 */
static gpointer
valent_device_manager_get_item (GListModel   *list,
                                unsigned int  position)
{
  ValentDeviceManager *self = VALENT_DEVICE_MANAGER (list);

  g_assert (VALENT_IS_DEVICE_MANAGER (self));

  if G_UNLIKELY (position >= self->devices->len)
    return NULL;

  return g_object_ref (g_ptr_array_index (self->devices, position));
}

static GType
valent_device_manager_get_item_type (GListModel *list)
{
  return VALENT_TYPE_DEVICE;
}

static unsigned int
valent_device_manager_get_n_items (GListModel *list)
{
  ValentDeviceManager *self = VALENT_DEVICE_MANAGER (list);

  g_assert (VALENT_IS_DEVICE_MANAGER (self));

  return self->devices->len;
}

static void
g_list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item = valent_device_manager_get_item;
  iface->get_item_type = valent_device_manager_get_item_type;
  iface->get_n_items = valent_device_manager_get_n_items;
}

/*
 * DBus
 */
typedef struct
{
  GDBusObjectManagerServer *manager;
  GDBusConnection          *connection;
  char                     *object_path;
  unsigned int              actions_id;
  unsigned int              menu_id;
} DeviceExport;

static void
device_export_free (gpointer data)
{
  DeviceExport *info = data;

  g_dbus_object_manager_server_unexport (info->manager, info->object_path);
  g_dbus_connection_unexport_action_group (info->connection, info->actions_id);
  g_dbus_connection_unexport_menu_model (info->connection, info->menu_id);

  g_clear_pointer (&info->object_path, g_free);
  g_clear_object (&info->connection);
  g_clear_object (&info->manager);
  g_free (info);
}

static void
valent_device_manager_export_device (ValentDeviceManager *self,
                                     ValentDevice        *device)
{
  GDBusObjectManager *manager = G_DBUS_OBJECT_MANAGER (self->dbus);
  const char *base_path = NULL;
  g_autofree char *escaped_id = NULL;
  g_autofree char *object_path = NULL;
  g_autoptr (GDBusObjectSkeleton) object = NULL;
  g_autoptr (GDBusInterfaceSkeleton) iface = NULL;
  DeviceExport *info;
  GActionGroup *action_group;
  GMenuModel *menu_model;

  VALENT_ENTRY;

  g_assert (VALENT_IS_DEVICE_MANAGER (self));
  g_assert (VALENT_IS_DEVICE (device));

  if (g_hash_table_contains (self->exports, device))
    VALENT_EXIT;

  base_path = g_dbus_object_manager_get_object_path (manager);
  escaped_id = g_dbus_escape_object_path (valent_device_get_id (device));
  object_path = g_strconcat (base_path, "/Device/", escaped_id, NULL);
  g_assert (g_variant_is_object_path (object_path));

  info = g_new0 (DeviceExport, 1);
  info->manager = g_object_ref (self->dbus);
  info->connection = g_dbus_object_manager_server_get_connection (self->dbus);
  info->object_path = g_steal_pointer (&object_path);

  object = g_dbus_object_skeleton_new (info->object_path);
  iface = valent_device_impl_new (device);
  g_dbus_object_skeleton_add_interface (object, iface);

  action_group = G_ACTION_GROUP (device);
  info->actions_id = g_dbus_connection_export_action_group (info->connection,
                                                            info->object_path,
                                                            action_group,
                                                            NULL);

  menu_model = valent_device_get_menu (device);
  info->menu_id = g_dbus_connection_export_menu_model (info->connection,
                                                       info->object_path,
                                                       menu_model,
                                                       NULL);

  g_dbus_object_manager_server_export (self->dbus, object);
  g_hash_table_insert (self->exports, device, g_steal_pointer (&info));

  VALENT_EXIT;
}

/*
 * Channel Services
 */
static gboolean
valent_device_manager_check_device (ValentDeviceManager *self,
                                    ValentDevice        *device)
{
  unsigned int n_unpaired = 0;

  if ((valent_device_get_state (device) & VALENT_DEVICE_STATE_PAIRED) != 0)
    return TRUE;

  for (unsigned int i = 0, len = self->devices->len; i < len; i++)
    {
      ValentDevice *check = g_ptr_array_index (self->devices, i);

      if ((valent_device_get_state (check) & VALENT_DEVICE_STATE_PAIRED) == 0)
        n_unpaired++;
    }

  if (n_unpaired >= DEVICE_UNPAIRED_MAX)
    {
      g_warning ("%s(): too many unpaired devices", G_STRFUNC);
      return FALSE;
    }

  return TRUE;
}

static void
on_channel (ValentChannelService *service,
            ValentChannel        *channel,
            ValentDeviceManager  *self)
{
  JsonNode *identity;
  ValentDevice *device;

  VALENT_ENTRY;

  g_assert (VALENT_IS_CHANNEL_SERVICE (service));
  g_assert (VALENT_IS_CHANNEL (channel));
  g_assert (VALENT_IS_DEVICE_MANAGER (self));

  if ((identity = valent_channel_get_peer_identity (channel)) == NULL)
    {
      g_warning ("%s(): %s missing peer identity",
                 G_STRFUNC,
                 G_OBJECT_TYPE_NAME (channel));
      VALENT_EXIT;
    }

  if ((device = valent_device_manager_ensure_device (self, identity)) == NULL)
    VALENT_EXIT;

  if (!valent_device_manager_check_device (self, device))
    {
      valent_object_destroy (VALENT_OBJECT (channel));
      VALENT_EXIT;
    }

  valent_device_add_channel (device, channel);

  VALENT_EXIT;
}

static void
on_service_state (ValentExtension     *extension,
                  GParamSpec          *pspec,
                  ValentDeviceManager *self)
{
  ValentPluginState state;
  g_autoptr (GError) error = NULL;

  state = valent_extension_plugin_state_check (extension, &error);
  switch ((ValentPluginState)state)
    {
    case VALENT_PLUGIN_STATE_ACTIVE:
      valent_channel_service_identify (VALENT_CHANNEL_SERVICE (extension), NULL);
      break;

    case VALENT_PLUGIN_STATE_INACTIVE:
      g_debug ("%s: %s", G_OBJECT_TYPE_NAME (extension), "inactive");
      break;

    case VALENT_PLUGIN_STATE_ERROR:
      g_warning ("%s: %s", G_OBJECT_TYPE_NAME (extension), error->message);
      break;
    }
}

static void
g_async_initable_init_async_cb (GAsyncInitable *initable,
                                GAsyncResult   *result,
                                gpointer        user_data)
{
  g_autoptr (GError) error = NULL;

  VALENT_ENTRY;

  g_assert (VALENT_IS_CHANNEL_SERVICE (initable));

  if (!g_async_initable_init_finish (initable, result, &error) &&
      !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    g_warning ("%s: %s", G_OBJECT_TYPE_NAME (initable), error->message);

  VALENT_EXIT;
}

static inline void
valent_device_manager_enable_plugin (ValentDeviceManager *self,
                                     ValentPlugin        *plugin)
{
  g_assert (VALENT_IS_DEVICE_MANAGER (self));
  g_assert (plugin != NULL);

  plugin->extension = peas_engine_create_extension (valent_get_plugin_engine (),
                                                    plugin->info,
                                                    VALENT_TYPE_CHANNEL_SERVICE,
                                                    "source",  self,
                                                    "context", plugin->context,
                                                    NULL);
  g_return_if_fail (G_IS_OBJECT (plugin->extension));

  g_signal_connect_object (plugin->extension,
                           "channel",
                           G_CALLBACK (on_channel),
                           self,
                           G_CONNECT_DEFAULT);
  g_signal_connect_object (plugin->extension,
                           "notify::plugin-state",
                           G_CALLBACK (on_service_state),
                           self,
                           G_CONNECT_DEFAULT);

  if (G_IS_ASYNC_INITABLE (plugin->extension))
    {
      plugin->cancellable = g_cancellable_new ();
      g_async_initable_init_async (G_ASYNC_INITABLE (plugin->extension),
                                   G_PRIORITY_DEFAULT,
                                   plugin->cancellable,
                                   (GAsyncReadyCallback)g_async_initable_init_async_cb,
                                   NULL);
    }
  else if (G_IS_INITABLE (plugin->extension))
    {
      GInitable *initable = G_INITABLE (plugin->extension);
      g_autoptr (GError) error = NULL;

      plugin->cancellable = g_cancellable_new ();
      if (!g_initable_init (initable, plugin->cancellable, &error) &&
          !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          g_warning ("%s: %s", G_OBJECT_TYPE_NAME (initable), error->message);
        }
    }
}

static inline void
valent_device_manager_disable_plugin (ValentDeviceManager *self,
                                      ValentPlugin        *plugin)
{
  g_assert (VALENT_IS_DEVICE_MANAGER (self));
  g_assert (plugin != NULL);
  g_return_if_fail (G_IS_OBJECT (plugin->extension));

  g_cancellable_cancel (plugin->cancellable);
  g_clear_object (&plugin->cancellable);

  if (plugin->extension != NULL)
    {
      valent_object_destroy (VALENT_OBJECT (plugin->extension));
      g_clear_object (&plugin->extension);
    }
}

static void
on_plugin_enabled_changed (ValentPlugin *plugin)
{
  g_assert (plugin != NULL);
  g_assert (VALENT_IS_DEVICE_MANAGER (plugin->parent));

  if (valent_plugin_get_enabled (plugin))
    valent_device_manager_enable_plugin (plugin->parent, plugin);
  else
    valent_device_manager_disable_plugin (plugin->parent, plugin);
}

static void
on_load_service (PeasEngine          *engine,
                 PeasPluginInfo      *info,
                 ValentDeviceManager *self)
{
  ValentPlugin *plugin;

  g_assert (PEAS_IS_ENGINE (engine));
  g_assert (info != NULL);
  g_assert (VALENT_IS_DEVICE_MANAGER (self));

  /* We're only interested in one GType */
  if (!peas_engine_provides_extension (engine, info, VALENT_TYPE_CHANNEL_SERVICE))
    return;

  VALENT_NOTE ("%s: %s",
               g_type_name (VALENT_TYPE_CHANNEL_SERVICE),
               peas_plugin_info_get_module_name (info));

  plugin = valent_plugin_new (self, self->plugins_context, info,
                              G_CALLBACK (on_plugin_enabled_changed));
  g_hash_table_insert (self->plugins, info, plugin);

  if (valent_plugin_get_enabled (plugin))
    valent_device_manager_enable_plugin (self, plugin);
}

static void
on_unload_service (PeasEngine          *engine,
                   PeasPluginInfo      *info,
                   ValentDeviceManager *self)
{
  g_assert (PEAS_IS_ENGINE (engine));
  g_assert (info != NULL);
  g_assert (VALENT_IS_DEVICE_MANAGER (self));

  /* We're only interested in one GType */
  if (!peas_engine_provides_extension (engine, info, VALENT_TYPE_CHANNEL_SERVICE))
    return;

  VALENT_NOTE ("%s: %s",
               g_type_name (VALENT_TYPE_CHANNEL_SERVICE),
               peas_plugin_info_get_module_name (info));

  g_hash_table_remove (self->plugins, info);
}

/*
 * Device Management
 */
static void
on_device_state (ValentDevice        *device,
                 GParamSpec          *pspec,
                 ValentDeviceManager *self)
{
  ValentDeviceState state = valent_device_get_state (device);

  if ((state & VALENT_DEVICE_STATE_CONNECTED) != 0 &&
      (state & VALENT_DEVICE_STATE_PAIRED) != 0)
    {
      g_autoptr (ValentChannel) channel = NULL;
      JsonNode *identity = NULL;

      channel = g_list_model_get_item (valent_device_get_channels (device), 0);
      identity = valent_channel_get_peer_identity (channel);
      json_object_set_object_member (json_node_get_object (self->state),
                                     valent_device_get_id (device),
                                     json_node_dup_object (identity));
    }
  else if ((state & VALENT_DEVICE_STATE_PAIRED) == 0)
    {
      json_object_remove_member (json_node_get_object (self->state),
                                 valent_device_get_id (device));

      if ((state & VALENT_DEVICE_STATE_CONNECTED) == 0)
        valent_device_manager_remove_device (self, device);
    }
}

static void
valent_device_manager_add_device (ValentDeviceManager *self,
                                  ValentDevice        *device)
{
  unsigned int position = 0;

  VALENT_ENTRY;

  g_assert (VALENT_IS_DEVICE_MANAGER (self));
  g_assert (VALENT_IS_DEVICE (device));

  if (g_ptr_array_find (self->devices, device, NULL))
    {
      g_warning ("Device \"%s\" already managed by \"%s\"",
                 valent_device_get_name (device),
                 G_OBJECT_TYPE_NAME (self));
      VALENT_EXIT;
    }

  g_signal_connect_object (device,
                           "notify::state",
                           G_CALLBACK (on_device_state),
                           self,
                           G_CONNECT_DEFAULT);

  position = self->devices->len;
  g_ptr_array_add (self->devices, g_object_ref (device));
  g_list_model_items_changed (G_LIST_MODEL (self), position, 0, 1);

  if (self->dbus != NULL)
    valent_device_manager_export_device (self, device);

  VALENT_EXIT;
}

static inline gboolean
find_device_by_id (gconstpointer a,
                   gconstpointer b)
{
  return g_str_equal (valent_device_get_id ((ValentDevice *)a), (const char *)b);
}

static ValentDevice *
valent_device_manager_ensure_device (ValentDeviceManager *self,
                                     JsonNode            *identity)
{
  const char *device_id;
  unsigned int position = 0;

  g_assert (VALENT_IS_DEVICE_MANAGER (self));
  g_assert (VALENT_IS_PACKET (identity));

  if (!valent_packet_get_string (identity, "deviceId", &device_id))
    {
      g_critical ("%s(): expected \"deviceId\" field holding a string",
                  G_STRFUNC);
      return NULL;
    }

  if (!valent_device_validate_id (device_id))
    {
      g_critical ("%s(): invalid device ID \"%s\"", G_STRFUNC, device_id);
      return NULL;
    }

  if (!g_ptr_array_find_with_equal_func (self->devices,
                                         device_id,
                                         find_device_by_id,
                                         &position))
    {
      g_autoptr (ValentContext) context = NULL;
      g_autoptr (ValentDevice) device = NULL;

      context = valent_context_new (self->context, "device", device_id);
      device = valent_device_new_full (identity, context);

      valent_device_manager_add_device (self, device);
      position = (self->devices->len - 1);
    }

  return g_ptr_array_index (self->devices, position);
}

static void
valent_device_manager_remove_device (ValentDeviceManager *self,
                                     ValentDevice        *device)
{
  unsigned int position = 0;

  VALENT_ENTRY;

  g_assert (VALENT_IS_DEVICE_MANAGER (self));
  g_assert (VALENT_IS_DEVICE (device));

  if (!g_ptr_array_find (self->devices, device, &position))
    {
      g_warning ("Device \"%s\" not managed by \"%s\"",
                 valent_device_get_name (device),
                 G_OBJECT_TYPE_NAME (self));
      VALENT_EXIT;
    }

  g_signal_handlers_disconnect_by_data (device, self);

  g_hash_table_remove (self->exports, device);
  g_ptr_array_remove_index (self->devices, position);
  g_list_model_items_changed (G_LIST_MODEL (self), position, 1, 0);

  VALENT_EXIT;
}

static void
valent_device_manager_load_state (ValentDeviceManager *self)
{
  JsonObjectIter iter;
  const char *device_id;
  JsonNode *identity;
  g_autoptr (GFile) path = NULL;
  g_autoptr (GTlsCertificate) certificate = NULL;
  g_autoptr (GError) error = NULL;

  g_assert (VALENT_IS_DEVICE_MANAGER (self));

  /* Ensure we're wiping old certificates with invalid device IDs. In the
   * unlikely event of an error, the channel service will re-generate it.
   *
   * TODO: remove this after a period of time
   */
  path = valent_context_get_config_file (self->context, ".");
  certificate = valent_certificate_new_sync (g_file_peek_path (path), NULL);
  if (certificate != NULL)
    {
      device_id = valent_certificate_get_common_name (certificate);
      if (!valent_device_validate_id (device_id))
        {
          g_autoptr (GFile) cert_file = NULL;
          g_autoptr (GFile) pkey_file = NULL;

          cert_file = valent_context_get_config_file (self->context,
                                                      "certificate.pem");
          g_file_delete (cert_file, NULL, NULL);
          pkey_file = valent_context_get_config_file (self->context,
                                                      "private.pem");
          g_file_delete (pkey_file, NULL, NULL);
        }
    }

  if (self->state == NULL)
    {
      g_autoptr (JsonParser) parser = NULL;
      g_autoptr (GFile) file = NULL;

      parser = json_parser_new ();
      file = valent_context_get_cache_file (self->context, "devices.json");
      if (json_parser_load_from_file (parser, g_file_peek_path (file), NULL))
        self->state = json_parser_steal_root (parser);

      if (self->state == NULL || !JSON_NODE_HOLDS_OBJECT (self->state))
        {
          g_clear_pointer (&self->state, json_node_unref);
          self->state = json_node_new (JSON_NODE_OBJECT);
          json_node_take_object (self->state, json_object_new ());
        }
    }

  json_object_iter_init (&iter, json_node_get_object (self->state));
  while (json_object_iter_next (&iter, &device_id, &identity))
    valent_device_manager_ensure_device (self, identity);
}

static void
valent_device_manager_save_state (ValentDeviceManager *self)
{
  g_autoptr (JsonGenerator) generator = NULL;
  g_autoptr (GFile) file = NULL;
  g_autoptr (GError) error = NULL;

  g_assert (VALENT_IS_DEVICE_MANAGER (self));

  for (unsigned int i = 0, len = self->devices->len; i < len; i++)
    {
      ValentDevice *device = g_ptr_array_index (self->devices, i);
      ValentDeviceState state = valent_device_get_state (device);

      if ((state & VALENT_DEVICE_STATE_PAIRED) == 0)
        {
          json_object_remove_member (json_node_get_object (self->state),
                                     valent_device_get_id (device));
        }
    }

  generator = g_object_new (JSON_TYPE_GENERATOR,
                            "pretty", TRUE,
                            "root",   self->state,
                            NULL);

  file = valent_context_get_cache_file (self->context, "devices.json");
  if (!json_generator_to_file (generator, g_file_peek_path (file), &error))
    g_warning ("%s(): %s", G_STRFUNC, error->message);
}

/*
 * GActions
 */
static void
device_action (GSimpleAction *action,
               GVariant      *parameter,
               gpointer       user_data)
{
  ValentDeviceManager *manager = valent_device_manager_get_default ();
  const char *device_id;
  const char *name;
  g_autoptr (GVariantIter) targetv = NULL;
  g_autoptr (GVariant) target = NULL;

  g_assert (VALENT_IS_DEVICE_MANAGER (manager));

  /* (<Valent.Device:id>, <Gio.Action:name>, [<GLib.Variant>]) */
  g_variant_get (parameter, "(&s&sav)", &device_id, &name, &targetv);
  g_variant_iter_next (targetv, "v", &target);

  for (unsigned int i = 0, len = manager->devices->len; i < len; i++)
    {
      ValentDevice *device = g_ptr_array_index (manager->devices, i);

      if (g_strcmp0 (device_id, valent_device_get_id (device)) == 0)
        {
          g_action_group_activate_action (G_ACTION_GROUP (device), name, target);
          break;
        }
    }
}

static const GActionEntry app_actions[] = {
  { "device",  device_action, "(ssav)", NULL, NULL },
};

/*
 * ValentApplicationPlugin
 */
static gboolean
valent_device_manager_dbus_register (ValentApplicationPlugin  *plugin,
                                     GDBusConnection          *connection,
                                     const char               *object_path,
                                     GError                  **error)
{
  ValentDeviceManager *self = VALENT_DEVICE_MANAGER (plugin);

  g_assert (VALENT_IS_DEVICE_MANAGER (self));
  g_assert (G_IS_DBUS_CONNECTION (connection));
  g_assert (g_variant_is_object_path (object_path));
  g_return_val_if_fail (self->dbus == NULL, TRUE);

  self->dbus = g_dbus_object_manager_server_new (object_path);
  g_dbus_object_manager_server_set_connection (self->dbus, connection);

  for (unsigned int i = 0, len = self->devices->len; i < len; i++)
    {
      ValentDevice *device = g_ptr_array_index (self->devices, i);

      valent_device_manager_export_device (self, device);
    }

  return TRUE;
}

static void
valent_device_manager_dbus_unregister (ValentApplicationPlugin *plugin,
                                       GDBusConnection         *connection,
                                       const char              *object_path)
{
  ValentDeviceManager *self = VALENT_DEVICE_MANAGER (plugin);

  g_assert (VALENT_IS_DEVICE_MANAGER (self));
  g_assert (G_IS_DBUS_CONNECTION (connection));
  g_assert (g_variant_is_object_path (object_path));
  g_return_if_fail (G_IS_DBUS_OBJECT_MANAGER (self->dbus));

  g_hash_table_remove_all (self->exports);
  g_dbus_object_manager_server_set_connection (self->dbus, NULL);
  g_clear_object (&self->dbus);
}

static void
valent_device_manager_shutdown (ValentApplicationPlugin *plugin)
{
  ValentDeviceManager *self = VALENT_DEVICE_MANAGER (plugin);
  unsigned int n_devices = 0;

  g_assert (VALENT_IS_DEVICE_MANAGER (self));
  g_return_if_fail (G_IS_CANCELLABLE (self->cancellable));

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);

  g_signal_handlers_disconnect_by_data (self->settings, self);
  g_clear_object (&self->settings);

  g_signal_handlers_disconnect_by_data (valent_get_plugin_engine (), self);
  g_hash_table_remove_all (self->plugins);
  valent_device_manager_save_state (self);

  n_devices = self->devices->len;
  for (unsigned int i = 0; i < n_devices; i++)
    {
      ValentDevice *device = g_ptr_array_index (self->devices, i);
      g_signal_handlers_disconnect_by_data (device, self);
    }

  g_ptr_array_remove_range (self->devices, 0, n_devices);
  g_list_model_items_changed (G_LIST_MODEL (self), 0, n_devices, 0);

  if (self == default_manager)
    {
      GApplication *application = g_application_get_default ();

      if (application != NULL)
        {
          for (size_t i = 0; i < G_N_ELEMENTS (app_actions); i++)
            {
              g_action_map_remove_action (G_ACTION_MAP (application),
                                          app_actions[i].name);
            }
        }
    }
}

static void
valent_device_manager_startup (ValentApplicationPlugin *plugin)
{
  ValentDeviceManager *self = VALENT_DEVICE_MANAGER (plugin);
  PeasEngine *engine = NULL;
  unsigned int n_plugins = 0;

  g_assert (VALENT_IS_DEVICE_MANAGER (self));
  g_return_if_fail (self->cancellable == NULL);

  self->cancellable = g_cancellable_new ();
  valent_device_manager_load_state (self);

  self->settings = g_settings_new ("ca.andyholmes.Valent");
  g_signal_connect_object (self->settings,
                           "changed::device-addresses",
                           G_CALLBACK (valent_device_manager_refresh),
                           self,
                           G_CONNECT_SWAPPED);

  engine = valent_get_plugin_engine ();
  g_signal_connect_object (engine,
                           "load-plugin",
                           G_CALLBACK (on_load_service),
                           self,
                           G_CONNECT_AFTER);
  g_signal_connect_object (engine,
                           "unload-plugin",
                           G_CALLBACK (on_unload_service),
                           self,
                           G_CONNECT_DEFAULT);

  n_plugins = g_list_model_get_n_items (G_LIST_MODEL (engine));
  for (unsigned int i = 0; i < n_plugins; i++)
    {
      g_autoptr (PeasPluginInfo) info = NULL;

      info = g_list_model_get_item (G_LIST_MODEL (engine), i);
      if (peas_plugin_info_is_loaded (info))
        on_load_service (engine, info, self);
    }

  if (self == default_manager)
    {
      GApplication *application = g_application_get_default ();

      if (application != NULL)
        {
          g_action_map_add_action_entries (G_ACTION_MAP (application),
                                           app_actions,
                                           G_N_ELEMENTS (app_actions),
                                           application);
        }
    }
}

/*
 * GObject
 */
static void
valent_device_manager_constructed (GObject *object)
{
  ValentDeviceManager *self = VALENT_DEVICE_MANAGER (object);

  G_OBJECT_CLASS (valent_device_manager_parent_class)->constructed (object);

  if (default_manager == NULL)
    {
      default_manager = self;
      g_object_add_weak_pointer (G_OBJECT (default_manager),
                                 (gpointer)&default_manager);
    }
}

static void
valent_device_manager_finalize (GObject *object)
{
  ValentDeviceManager *self = VALENT_DEVICE_MANAGER (object);

  g_clear_pointer (&self->exports, g_hash_table_unref);
  g_clear_pointer (&self->plugins, g_hash_table_unref);
  g_clear_pointer (&self->plugins_context, g_object_unref);
  g_clear_pointer (&self->devices, g_ptr_array_unref);
  g_clear_pointer (&self->state, json_node_unref);
  g_clear_object (&self->context);

  G_OBJECT_CLASS (valent_device_manager_parent_class)->finalize (object);
}

static void
valent_device_manager_class_init (ValentDeviceManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentApplicationPluginClass *plugin_class = VALENT_APPLICATION_PLUGIN_CLASS (klass);

  object_class->constructed = valent_device_manager_constructed;
  object_class->finalize = valent_device_manager_finalize;

  plugin_class->dbus_register = valent_device_manager_dbus_register;
  plugin_class->dbus_unregister = valent_device_manager_dbus_unregister;
  plugin_class->shutdown = valent_device_manager_shutdown;
  plugin_class->startup = valent_device_manager_startup;
}

static void
valent_device_manager_init (ValentDeviceManager *self)
{
  self->context = valent_context_new (NULL, NULL, NULL);
  self->devices = g_ptr_array_new_with_free_func (_valent_object_deref);
  self->exports = g_hash_table_new_full (NULL, NULL, NULL, device_export_free);
  self->plugins = g_hash_table_new_full (NULL, NULL, NULL, valent_plugin_free);
  self->plugins_context = valent_context_new (self->context, "network", NULL);
}

/**
 * valent_device_manager_get_default:
 *
 * Get the default [class@Valent.DeviceManager].
 *
 * Returns: (transfer none) (not nullable): a `ValentDeviceManager`
 *
 * Since: 1.0
 */
ValentDeviceManager *
valent_device_manager_get_default (void)
{
  if (default_manager == NULL)
    return g_object_new (VALENT_TYPE_DEVICE_MANAGER, NULL);

  return default_manager;
}

/**
 * valent_device_manager_refresh:
 * @manager: a `ValentDeviceManager`
 *
 * Refresh the available devices.
 *
 * This method calls [method@Valent.ChannelService.identify] for each enabled
 * service, requesting it to announce itself on its respective network.
 *
 * Since: 1.0
 */
void
valent_device_manager_refresh (ValentDeviceManager *manager)
{
  GHashTableIter iter;
  ValentPlugin *plugin;
  g_auto (GStrv) addresses = NULL;

  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_DEVICE_MANAGER (manager));

  if (manager->cancellable == NULL)
    VALENT_EXIT;

  addresses = g_settings_get_strv (manager->settings, "device-addresses");

  g_hash_table_iter_init (&iter, manager->plugins);
  while (g_hash_table_iter_next (&iter, NULL, (void **)&plugin))
    {
      if (plugin->extension == NULL)
        continue;

      valent_channel_service_identify (VALENT_CHANNEL_SERVICE (plugin->extension),
                                       NULL);

      for (size_t i = 0; addresses[i] != NULL; i++)
        {
          valent_channel_service_identify (VALENT_CHANNEL_SERVICE (plugin->extension),
                                           addresses[i]);
        }
    }

  VALENT_EXIT;
}

