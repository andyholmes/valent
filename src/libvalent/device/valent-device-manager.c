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
  ValentObject              parent_instance;

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

static void   g_initable_iface_init       (GInitableIface      *iface);
static void   g_async_initable_iface_init (GAsyncInitableIface *iface);
static void   g_list_model_iface_init     (GListModelInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (ValentDeviceManager, valent_device_manager, VALENT_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, g_initable_iface_init)
                               G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE, g_async_initable_iface_init)
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, g_list_model_iface_init))

enum {
  PROP_0,
  PROP_NAME,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };


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
  g_return_if_fail (PEAS_IS_EXTENSION (plugin->extension));

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
  g_return_if_fail (PEAS_IS_EXTENSION (plugin->extension));

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
 * GInitable
 */
static gboolean
valent_device_manager_initable_init (GInitable     *initable,
                                     GCancellable  *cancellable,
                                     GError       **error)
{
  ValentDeviceManager *self = VALENT_DEVICE_MANAGER (initable);
  const char *path = NULL;

  g_assert (VALENT_IS_DEVICE_MANAGER (self));

  /* Generate certificate */
  path = valent_context_get_config_path (self->context);
  self->certificate = valent_certificate_new_sync (path, error);

  if (self->certificate == NULL)
    return FALSE;

  self->id = valent_certificate_get_common_name (self->certificate);

  return TRUE;
}

static void
g_initable_iface_init (GInitableIface *iface)
{
  iface->init = valent_device_manager_initable_init;
}

/*
 * GAsyncInitable
 */
static void
valent_certificate_new_cb (GObject      *object,
                           GAsyncResult *result,
                           gpointer      user_data)
{
  g_autoptr (GTask) task = G_TASK (user_data);
  ValentDeviceManager *self = g_task_get_source_object (task);
  GError *error = NULL;

  self->certificate = valent_certificate_new_finish (result, &error);

  if (error != NULL)
    return g_task_return_error (task, error);

  self->id = valent_certificate_get_common_name (self->certificate);
  g_task_return_boolean (task, TRUE);
}

static void
valent_device_manager_init_async (GAsyncInitable      *initable,
                                  int                  io_priority,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data)
{
  ValentDeviceManager *self = VALENT_DEVICE_MANAGER (initable);
  g_autoptr (GTask) task = NULL;
  g_autoptr (GCancellable) destroy = NULL;
  const char *path = NULL;

  g_assert (VALENT_IS_DEVICE_MANAGER (self));

  /* Cancel initialization if the object is destroyed */
  destroy = valent_object_attach_cancellable (VALENT_OBJECT (initable),
                                              cancellable);

  task = g_task_new (initable, destroy, callback, user_data);
  g_task_set_priority (task, io_priority);
  g_task_set_source_tag (task, valent_device_manager_init_async);

  path = valent_context_get_config_path (self->context);
  valent_certificate_new (path,
                          destroy,
                          valent_certificate_new_cb,
                          g_steal_pointer (&task));
}

static void
g_async_initable_iface_init (GAsyncInitableIface *iface)
{
  iface->init_async = valent_device_manager_init_async;
}

/*
 * GObject
 */
static void
valent_device_manager_dispose (GObject *object)
{
  ValentDeviceManager *self = VALENT_DEVICE_MANAGER (object);

  valent_device_manager_stop (self);
  valent_device_manager_unexport (self);

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

  object_class->dispose = valent_device_manager_dispose;
  object_class->finalize = valent_device_manager_finalize;
  object_class->get_property = valent_device_manager_get_property;
  object_class->set_property = valent_device_manager_set_property;

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
 * valent_device_manager_new_sync:
 * @cancellable: (nullable): a #GCancellable
 * @error: (nullable): a #GError
 *
 * Create a new #ValentDeviceManager.
 *
 * Returns: (transfer full) (nullable): a #ValentDeviceManager
 *
 * Since: 1.0
 */
ValentDeviceManager *
valent_device_manager_new_sync (GCancellable  *cancellable,
                                GError       **error)
{
  GInitable *manager;

  g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  manager = g_initable_new (VALENT_TYPE_DEVICE_MANAGER,
                            cancellable,
                            error,
                            NULL);

  if (manager == NULL)
    return NULL;

  return VALENT_DEVICE_MANAGER (manager);
}

/**
 * valent_device_manager_new:
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): a #GAsyncReadyCallback
 * @user_data: (closure): user supplied data
 *
 * Asynchronously create a new #ValentDeviceManager.
 *
 * When the manager is ready @callback will be invoked and you can use
 * [ctor@Valent.DeviceManager.new_finish] to get the result.
 *
 * Since: 1.0
 */
void
valent_device_manager_new (GCancellable        *cancellable,
                           GAsyncReadyCallback  callback,
                           gpointer             user_data)
{
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  g_async_initable_new_async (VALENT_TYPE_DEVICE_MANAGER,
                              G_PRIORITY_DEFAULT,
                              cancellable,
                              callback,
                              user_data,
                              NULL);
}

/**
 * valent_device_manager_new_finish:
 * @result: a #GAsyncResult
 * @error: (nullable): a #GError
 *
 * Finish an operation started by [func@Valent.DeviceManager.new].
 *
 * Returns: (transfer full) (nullable): a #ValentDeviceManager
 *
 * Since: 1.0
 */
ValentDeviceManager *
valent_device_manager_new_finish (GAsyncResult  *result,
                                  GError       **error)
{
  g_autoptr (GObject) source_object = NULL;
  GObject *manager;

  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  source_object = g_async_result_get_source_object (result);
  manager = g_async_initable_new_finish (G_ASYNC_INITABLE (source_object),
                                         result,
                                         error);

  if (manager == NULL)
    return NULL;

  return VALENT_DEVICE_MANAGER (manager);
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

/**
 * valent_device_manager_start:
 * @manager: a #ValentDeviceManager
 *
 * Start managing devices.
 *
 * Calling this method causes @manager to load all [class@Valent.ChannelService]
 * implementations known to the [class@Peas.Engine], allowing new connections to
 * be opened.
 *
 * In a typical [class@Gio.Application], this should be called in the
 * [vfunc@Gio.Application.startup] override, after chaining up.
 *
 * Since: 1.0
 */
void
valent_device_manager_start (ValentDeviceManager *manager)
{
  PeasEngine *engine = NULL;
  const GList *plugins = NULL;

  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_DEVICE_MANAGER (manager));

  /* We're already started */
  if (manager->cancellable != NULL)
    VALENT_EXIT;

  manager->cancellable = g_cancellable_new ();

  /* Load devices */
  valent_device_manager_load_state (manager);

  /* Setup services */
  engine = valent_get_plugin_engine ();
  plugins = peas_engine_get_plugin_list (engine);

  for (const GList *iter = plugins; iter; iter = iter->next)
    {
      if (peas_plugin_info_is_loaded (iter->data))
        on_load_service (engine, iter->data, manager);
    }

  g_signal_connect_object (engine,
                           "load-plugin",
                           G_CALLBACK (on_load_service),
                           manager,
                           G_CONNECT_AFTER);

  g_signal_connect_object (engine,
                           "unload-plugin",
                           G_CALLBACK (on_unload_service),
                           manager,
                           0);

  VALENT_EXIT;
}

/**
 * valent_device_manager_stop:
 * @manager: a #ValentDeviceManager
 *
 * Stop managing devices.
 *
 * Calling this method causes @manager to unload all
 * [class@Valent.ChannelService] implementations, preventing any new connections
 * from being opened.
 *
 * In a typical [class@Gio.Application], this should be called in the
 * [vfunc@Gio.Application.shutdown] override, before chaining up.
 *
 * Since: 1.0
 */
void
valent_device_manager_stop (ValentDeviceManager *manager)
{
  PeasEngine *engine = NULL;
  unsigned int n_devices = 0;

  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_DEVICE_MANAGER (manager));

  /* We're already stopped */
  if (manager->cancellable == NULL)
    VALENT_EXIT;

  /* Cancel any running operations */
  g_cancellable_cancel (manager->cancellable);
  g_clear_object (&manager->cancellable);

  /* Stop and remove services */
  engine = valent_get_plugin_engine ();
  g_signal_handlers_disconnect_by_data (engine, manager);
  g_hash_table_remove_all (manager->plugins);

  /* Remove any devices */
  n_devices = manager->devices->len;

  for (unsigned int i = 0; i < n_devices; i++)
    {
      ValentDevice *device = g_ptr_array_index (manager->devices, i);
      g_signal_handlers_disconnect_by_data (device, manager);
    }

  g_ptr_array_remove_range (manager->devices, 0, n_devices);
  g_list_model_items_changed (G_LIST_MODEL (manager), 0, n_devices, 0);

  valent_device_manager_save_state (manager);

  VALENT_EXIT;
}

/**
 * valent_device_manager_export:
 * @manager: a #ValentDeviceManager
 * @connection: a #GDBusConnection
 * @object_path: a D-Bus object path
 *
 * Export the manager on D-Bus.
 *
 * Calling this method exports @manager and all managed [class@Valent.Device]
 * objects on @connection at @object_path.
 *
 * In a typical [class@Gio.Application], this should be called in the
 * [vfunc@Gio.Application.dbus_register] override, after chaining up.
 *
 * Since: 1.0
 */
void
valent_device_manager_export (ValentDeviceManager *manager,
                              GDBusConnection     *connection,
                              const char          *object_path)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_DEVICE_MANAGER (manager));
  g_return_if_fail (G_IS_DBUS_CONNECTION (connection));
  g_return_if_fail (g_variant_is_object_path (object_path));

  if (manager->dbus != NULL)
    VALENT_EXIT;

  manager->dbus = g_dbus_object_manager_server_new (object_path);
  g_dbus_object_manager_server_set_connection (manager->dbus, connection);

  for (unsigned int i = 0, len = manager->devices->len; i < len; i++)
    {
      ValentDevice *device = g_ptr_array_index (manager->devices, i);

      valent_device_manager_export_device (manager, device);
    }

  VALENT_EXIT;
}

/**
 * valent_device_manager_unexport:
 * @manager: a #ValentDeviceManager
 *
 * Unexport the manager from D-Bus.
 *
 * Calling this method unexports @manager from D-Bus, including all managed
 * [class@Valent.Device].
 *
 * In a typical [class@Gio.Application], this should be called in the
 * [vfunc@Gio.Application.dbus_unregister] override, before chaining up.
 *
 * Since: 1.0
 */
void
valent_device_manager_unexport (ValentDeviceManager *manager)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_DEVICE_MANAGER (manager));

  if (manager->dbus == NULL)
    VALENT_EXIT;

  for (unsigned int i = 0, len = manager->devices->len; i < len; i++)
    {
      ValentDevice *device = g_ptr_array_index (manager->devices, i);

      valent_device_manager_unexport_device (manager, device);
    }

  g_dbus_object_manager_server_set_connection (manager->dbus, NULL);
  g_clear_object (&manager->dbus);

  VALENT_EXIT;
}

