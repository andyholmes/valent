// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-device-manager"

#include "config.h"

#include <gio/gio.h>
#include <libvalent-core.h>

#include "valent-channel.h"
#include "valent-channel-service.h"
#include "valent-component-private.h"
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
 * #ValentDeviceManager manages the available [class@Valent.Device] objects,
 * connecting them when [signal@Valent.ChannelService::channel] is emitted by an
 * enabled implementation, exporting them on D-Bus and removing them when they
 * become unavailable.
 *
 * Since: 1.0
 */

struct _ValentDeviceManager
{
  ValentApplicationPlugin   parent_instance;

  GCancellable             *cancellable;
  ValentContext            *context;
  GTlsCertificate          *certificate;
  const char               *id;
  char                     *name;

  GPtrArray                *devices;
  GHashTable               *plugins;
  ValentContext            *plugins_context;
  JsonNode                 *state;

  GDBusObjectManagerServer *dbus;
  GHashTable               *exported;
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

enum {
  PROP_0,
  PROP_NAME,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

static ValentDeviceManager *default_manager = NULL;


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
  GDBusConnection *connection;
  char            *object_path;
  unsigned int     actions_id;
  unsigned int     menu_id;
} ExportedDevice;

static char *
valent_device_manager_get_device_object_path (ValentDeviceManager *self,
                                              ValentDevice        *device)
{
  GDBusObjectManager *object_manager;
  GString *object_path = NULL;
  const char *base_path = NULL;
  const char *id = NULL;

  g_assert (VALENT_IS_DEVICE_MANAGER (self));
  g_assert (VALENT_IS_DEVICE (device));

  object_manager = G_DBUS_OBJECT_MANAGER (self->dbus);
  base_path = g_dbus_object_manager_get_object_path (object_manager);

  object_path = g_string_new (base_path);
  g_string_append (object_path, "/Device/");

  id = valent_device_get_id (device);

  while (*id)
    {
      if G_LIKELY (g_ascii_isalnum (*id))
        g_string_append_c (object_path, *id);
      else
        g_string_append_c (object_path, '_');

      id++;
    }

  return g_string_free (object_path, FALSE);
}

static void
valent_device_manager_export_device (ValentDeviceManager *self,
                                     ValentDevice        *device)
{
  g_autoptr (GDBusObjectSkeleton) object = NULL;
  g_autoptr (GDBusInterfaceSkeleton) iface = NULL;
  ExportedDevice *info;
  GActionGroup *action_group;
  GMenuModel *menu_model;

  VALENT_ENTRY;

  g_assert (VALENT_IS_DEVICE_MANAGER (self));
  g_assert (VALENT_IS_DEVICE (device));

  if (g_hash_table_contains (self->exported, device))
    VALENT_EXIT;

  info = g_new0 (ExportedDevice, 1);
  info->connection = g_dbus_object_manager_server_get_connection (self->dbus);
  info->object_path = valent_device_manager_get_device_object_path (self,
                                                                    device);

  /* Export the ValentDevice, GActionGroup and GMenuModel interfaces on the same
   * connection and path */
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
  g_hash_table_insert (self->exported, device, info);

  VALENT_EXIT;
}

static void
valent_device_manager_unexport_device (ValentDeviceManager *self,
                                       ValentDevice        *device)
{
  gpointer data;
  ExportedDevice *info = NULL;

  VALENT_ENTRY;

  g_assert (VALENT_IS_DEVICE_MANAGER (self));
  g_assert (VALENT_IS_DEVICE (device));

  if (!g_hash_table_steal_extended (self->exported, device, NULL, &data))
    VALENT_EXIT;

  info = (ExportedDevice *)data;

  g_dbus_object_manager_server_unexport (self->dbus, info->object_path);
  g_dbus_connection_unexport_action_group (info->connection, info->actions_id);
  g_dbus_connection_unexport_menu_model (info->connection, info->menu_id);

  g_clear_pointer (&info->object_path, g_free);
  g_clear_object (&info->connection);
  g_free (info);

  VALENT_EXIT;
}

/*
 * Channel Services
 */
static void
manager_plugin_free (gpointer data)
{
  ValentPlugin *plugin = data;

  if (plugin->extension != NULL)
    {
      g_signal_handlers_disconnect_by_data (plugin->extension, plugin->parent);
      valent_object_destroy (VALENT_OBJECT (plugin->extension));
    }

  g_clear_pointer (&plugin, valent_plugin_free);
}

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
    VALENT_EXIT;

  valent_device_set_channel (device, channel);

  VALENT_EXIT;
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
                                                    "context", plugin->context,
                                                    "id",      self->id,
                                                    "name",    self->name,
                                                    NULL);
  g_return_if_fail (G_IS_OBJECT (plugin->extension));

  g_object_bind_property (self,              "name",
                          plugin->extension, "name",
                          G_BINDING_DEFAULT);

  g_signal_connect_object (plugin->extension,
                           "channel",
                           G_CALLBACK (on_channel),
                           self, 0);

  if (G_IS_ASYNC_INITABLE (plugin->extension))
    {
      g_autoptr (GCancellable) destroy = NULL;

      /* Use a cancellable in case the plugin is unloaded before the operation
       * completes. Chain to the manager in case it's destroyed. */
      plugin->cancellable = g_cancellable_new ();
      destroy = valent_object_chain_cancellable (VALENT_OBJECT (self),
                                                 plugin->cancellable);

      g_async_initable_init_async (G_ASYNC_INITABLE (plugin->extension),
                                   G_PRIORITY_DEFAULT,
                                   destroy,
                                   (GAsyncReadyCallback)g_async_initable_init_async_cb,
                                   NULL);
    }
}

static inline void
valent_device_manager_disable_plugin (ValentDeviceManager *self,
                                      ValentPlugin        *plugin)
{
  g_assert (VALENT_IS_DEVICE_MANAGER (self));
  g_assert (plugin != NULL);
  g_return_if_fail (G_IS_OBJECT (plugin->extension));

  if (plugin->extension != NULL)
    {
      g_signal_handlers_disconnect_by_data (plugin->extension, self);
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
static gboolean
valent_device_manager_remove_device_main (gpointer data)
{
  g_object_unref (VALENT_DEVICE (data));

  return G_SOURCE_REMOVE;
}

static void
on_device_state (ValentDevice        *device,
                 GParamSpec          *pspec,
                 ValentDeviceManager *self)
{
  ValentDeviceState state = valent_device_get_state (device);

  /* Devices that become connected and paired are remembered */
  if ((state & VALENT_DEVICE_STATE_CONNECTED) != 0 &&
      (state & VALENT_DEVICE_STATE_PAIRED) != 0)
    {
      g_autoptr (ValentChannel) channel = NULL;
      JsonNode *identity = NULL;

      if ((channel = valent_device_ref_channel (device)) == NULL)
        return;

      identity = valent_channel_get_peer_identity (channel);

      json_object_set_object_member (json_node_get_object (self->state),
                                     valent_device_get_id (device),
                                     json_node_dup_object (identity));
    }

  /* Devices that become disconnected and unpaired are forgotten */
  if ((state & VALENT_DEVICE_STATE_CONNECTED) == 0 &&
      (state & VALENT_DEVICE_STATE_PAIRED) == 0)
    {
      json_object_remove_member (json_node_get_object (self->state),
                                 valent_device_get_id (device));
      valent_device_manager_remove_device (self, device);
    }
}

static ValentDevice *
valent_device_manager_lookup (ValentDeviceManager *manager,
                              const char          *id)
{
  g_assert (VALENT_IS_DEVICE_MANAGER (manager));
  g_assert (id != NULL);

  for (unsigned int i = 0, len = manager->devices->len; i < len; i++)
    {
      ValentDevice *device = g_ptr_array_index (manager->devices, i);

      if (strcmp (id, valent_device_get_id (device)) == 0)
        return device;
    }

  return NULL;
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
    VALENT_EXIT;

  g_signal_connect_object (device,
                           "notify::state",
                           G_CALLBACK (on_device_state),
                           self,
                           0);

  position = self->devices->len;
  g_ptr_array_add (self->devices, g_object_ref (device));
  g_list_model_items_changed (G_LIST_MODEL (self), position, 0, 1);

  if (self->dbus != NULL)
    valent_device_manager_export_device (self, device);

  VALENT_EXIT;
}

static void
valent_device_manager_remove_device (ValentDeviceManager *manager,
                                     ValentDevice        *device)
{
  unsigned int position = 0;

  VALENT_ENTRY;

  g_assert (VALENT_IS_DEVICE_MANAGER (manager));
  g_assert (VALENT_IS_DEVICE (device));

  g_object_ref (device);

  if (g_ptr_array_find (manager->devices, device, &position))
    {
      valent_device_manager_unexport_device (manager, device);
      g_signal_handlers_disconnect_by_data (device, manager);
      g_ptr_array_remove_index (manager->devices, position);
      g_list_model_items_changed (G_LIST_MODEL (manager), position, 1, 0);

      // HACK: we are in a signal handler of a device's `notify::state`
      //       emission, so if we drop the last reference the emitting object
      //       and other handlers may be setup for a use-after-free error.
      g_idle_add (valent_device_manager_remove_device_main, g_object_ref (device));
    }

  g_object_unref (device);

  VALENT_EXIT;
}

static ValentDevice *
valent_device_manager_ensure_device (ValentDeviceManager *manager,
                                     JsonNode            *identity)
{
  const char *device_id;

  g_assert (VALENT_IS_DEVICE_MANAGER (manager));
  g_assert (VALENT_IS_PACKET (identity));

  if (!valent_packet_get_string (identity, "deviceId", &device_id))
    {
      g_debug ("%s(): expected \"deviceId\" field holding a string",
               G_STRFUNC);
      return NULL;
    }

  if (valent_device_manager_lookup (manager, device_id) == NULL)
    {
      g_autoptr (ValentContext) context = NULL;
      g_autoptr (ValentDevice) device = NULL;

      context = valent_context_new (manager->context, "device", device_id);
      device = valent_device_new_full (identity, context);

      valent_device_manager_add_device (manager, device);
    }

  return valent_device_manager_lookup (manager, device_id);
}

static void
valent_device_manager_load_state (ValentDeviceManager *self)
{
  JsonObjectIter iter;
  const char *device_id;
  JsonNode *identity;

  g_assert (VALENT_IS_DEVICE_MANAGER (self));

  if (self->state == NULL)
    {
      g_autoptr (JsonParser) parser = NULL;
      g_autoptr (GFile) file = NULL;

      file = valent_context_get_cache_file (self->context, "devices.json");

      /* Try to load the state file */
      parser = json_parser_new ();

      if (json_parser_load_from_file (parser, g_file_peek_path (file), NULL))
        self->state = json_parser_steal_root (parser);

      if (self->state == NULL || !JSON_NODE_HOLDS_OBJECT (self->state))
        {
          g_clear_pointer (&self->state, json_node_unref);
          self->state = json_node_new (JSON_NODE_OBJECT);
          json_node_take_object (self->state, json_object_new ());
        }
    }

  /* Load devices */
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

  if (self->dbus != NULL)
    return TRUE;

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
  g_assert (connection == NULL || G_IS_DBUS_CONNECTION (connection));
  g_assert (object_path == NULL || g_variant_is_object_path (object_path));

  if (self->dbus == NULL)
    return;

  for (unsigned int i = 0, len = self->devices->len; i < len; i++)
    {
      ValentDevice *device = g_ptr_array_index (self->devices, i);

      valent_device_manager_unexport_device (self, device);
    }

  g_dbus_object_manager_server_set_connection (self->dbus, NULL);
  g_clear_object (&self->dbus);
}

static void
valent_device_manager_shutdown (ValentApplicationPlugin *plugin)
{
  ValentDeviceManager *self = VALENT_DEVICE_MANAGER (plugin);
  PeasEngine *engine = NULL;
  unsigned int n_devices = 0;

  g_assert (VALENT_IS_DEVICE_MANAGER (self));

  /* We're already stopped */
  if (self->cancellable == NULL)
    return;

  /* Cancel any running operations */
  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);

  /* Stop and remove services */
  engine = valent_get_plugin_engine ();
  g_signal_handlers_disconnect_by_data (engine, self);
  g_hash_table_remove_all (self->plugins);

  /* Remove any devices */
  n_devices = self->devices->len;

  for (unsigned int i = 0; i < n_devices; i++)
    {
      ValentDevice *device = g_ptr_array_index (self->devices, i);
      g_signal_handlers_disconnect_by_data (device, self);
    }

  g_ptr_array_remove_range (self->devices, 0, n_devices);
  g_list_model_items_changed (G_LIST_MODEL (self), 0, n_devices, 0);

  valent_device_manager_save_state (self);

  /* Remove actions from the `app` group, if available */
  if (self == default_manager)
    {
      GApplication *application = g_application_get_default ();

      if (application != NULL)
        {
          for (unsigned int i = 0; i < G_N_ELEMENTS (app_actions); i++)
            g_action_map_remove_action (G_ACTION_MAP (application),
                                        app_actions[i].name);

        }
    }
}

static void
valent_device_manager_startup (ValentApplicationPlugin *plugin)
{
  ValentDeviceManager *self = VALENT_DEVICE_MANAGER (plugin);

  PeasEngine *engine = NULL;
  const GList *plugins = NULL;

  g_assert (VALENT_IS_DEVICE_MANAGER (self));

  /* We're already started */
  if (self->cancellable != NULL)
    return;;

  self->cancellable = g_cancellable_new ();

  /* Load devices */
  valent_device_manager_load_state (self);

  /* Setup services */
  engine = valent_get_plugin_engine ();
  plugins = peas_engine_get_plugin_list (engine);

  for (const GList *iter = plugins; iter; iter = iter->next)
    {
      if (peas_plugin_info_is_loaded (iter->data))
        on_load_service (engine, iter->data, self);
    }

  g_signal_connect_object (engine,
                           "load-plugin",
                           G_CALLBACK (on_load_service),
                           self,
                           G_CONNECT_AFTER);

  g_signal_connect_object (engine,
                           "unload-plugin",
                           G_CALLBACK (on_unload_service),
                           self,
                           0);

  /* Add actions to the `app` group, if available */
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
  g_autoptr (GError) error = NULL;
  const char *path = NULL;

  g_assert (VALENT_IS_DEVICE_MANAGER (self));

  /* Generate certificate */
  path = valent_context_get_config_path (self->context);

  if ((self->certificate = valent_certificate_new_sync (path, &error)) != NULL)
    self->id = valent_certificate_get_common_name (self->certificate);

  if (self->id == NULL)
    {
      self->id = g_uuid_string_random ();
      g_warning ("%s(): %s", G_STRFUNC, error->message);
    }

  G_OBJECT_CLASS (valent_device_manager_parent_class)->constructed (object);
}

static void
valent_device_manager_dispose (GObject *object)
{
  ValentDeviceManager *self = VALENT_DEVICE_MANAGER (object);

  valent_device_manager_shutdown (VALENT_APPLICATION_PLUGIN (self));
  valent_device_manager_dbus_unregister (VALENT_APPLICATION_PLUGIN (self),
                                         NULL,
                                         NULL);

  G_OBJECT_CLASS (valent_device_manager_parent_class)->dispose (object);
}

static void
valent_device_manager_finalize (GObject *object)
{
  ValentDeviceManager *self = VALENT_DEVICE_MANAGER (object);

  g_clear_pointer (&self->exported, g_hash_table_unref);
  g_clear_pointer (&self->plugins, g_hash_table_unref);
  g_clear_pointer (&self->plugins_context, g_object_unref);
  g_clear_pointer (&self->devices, g_ptr_array_unref);
  g_clear_pointer (&self->state, json_node_unref);

  g_clear_object (&self->certificate);
  g_clear_object (&self->context);
  g_clear_pointer (&self->name, g_free);

  G_OBJECT_CLASS (valent_device_manager_parent_class)->finalize (object);
}

static void
valent_device_manager_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  ValentDeviceManager *self = VALENT_DEVICE_MANAGER (object);

  switch (prop_id)
    {
    case PROP_NAME:
      g_value_set_string (value, self->name);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_device_manager_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  ValentDeviceManager *self = VALENT_DEVICE_MANAGER (object);

  switch (prop_id)
    {
    case PROP_NAME:
      valent_device_manager_set_name (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_device_manager_class_init (ValentDeviceManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentApplicationPluginClass *plugin_class = VALENT_APPLICATION_PLUGIN_CLASS (klass);

  object_class->constructed = valent_device_manager_constructed;
  object_class->dispose = valent_device_manager_dispose;
  object_class->finalize = valent_device_manager_finalize;
  object_class->get_property = valent_device_manager_get_property;
  object_class->set_property = valent_device_manager_set_property;

  plugin_class->dbus_register = valent_device_manager_dbus_register;
  plugin_class->dbus_unregister = valent_device_manager_dbus_unregister;
  plugin_class->shutdown = valent_device_manager_shutdown;
  plugin_class->startup = valent_device_manager_startup;

  /**
   * ValentDeviceManager:name: (getter get_name) (setter set_name)
   *
   * The display name of the local device.
   *
   * Since: 1.0
   */
  properties [PROP_NAME] =
    g_param_spec_string ("name", NULL, NULL,
                         "Valent",
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
valent_device_manager_init (ValentDeviceManager *self)
{
  self->context = valent_context_new (NULL, NULL, NULL);
  self->devices = g_ptr_array_new_with_free_func (g_object_unref);
  self->exported = g_hash_table_new (NULL, NULL);
  self->plugins = g_hash_table_new_full (NULL, NULL, NULL, manager_plugin_free);
  self->plugins_context = valent_context_new (self->context, "network", NULL);
}

/**
 * valent_device_manager_get_default:
 *
 * Get the default [class@Valent.DeviceManager].
 *
 * Returns: (transfer none) (not nullable): a #ValentDeviceManager
 *
 * Since: 1.0
 */
ValentDeviceManager *
valent_device_manager_get_default (void)
{
  if (default_manager == NULL)
    {
      default_manager = g_object_new (VALENT_TYPE_DEVICE_MANAGER,
                                      NULL);

      g_object_add_weak_pointer (G_OBJECT (default_manager),
                                 (gpointer)&default_manager);
    }

  return default_manager;
}

/**
 * valent_device_manager_get_name: (get-property name)
 * @manager: a #ValentDeviceManager
 *
 * Get the display name of the local device.
 *
 * Returns: (transfer none): the local display name
 *
 * Since: 1.0
 */
const char *
valent_device_manager_get_name (ValentDeviceManager *manager)
{
  g_return_val_if_fail (VALENT_IS_DEVICE_MANAGER (manager), NULL);

  return manager->name;
}

/**
 * valent_device_manager_set_name: (set-property name)
 * @manager: a #ValentDeviceManager
 * @name: (not nullable): a display name
 *
 * Set the display name of the local device to @name.
 *
 * Since: 1.0
 */
void
valent_device_manager_set_name (ValentDeviceManager *manager,
                                const char          *name)
{
  g_return_if_fail (VALENT_IS_DEVICE_MANAGER (manager));
  g_return_if_fail (name != NULL && *name != '\0');

  if (valent_set_string (&manager->name, name))
    g_object_notify_by_pspec (G_OBJECT (manager), properties [PROP_NAME]);
}

/**
 * valent_device_manager_refresh:
 * @manager: a #ValentDeviceManager
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

  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_DEVICE_MANAGER (manager));

  g_hash_table_iter_init (&iter, manager->plugins);

  while (g_hash_table_iter_next (&iter, NULL, (void **)&plugin))
    {
      if (plugin->extension == NULL)
        continue;

      valent_channel_service_identify (VALENT_CHANNEL_SERVICE (plugin->extension),
                                       NULL);
    }

  VALENT_EXIT;
}

