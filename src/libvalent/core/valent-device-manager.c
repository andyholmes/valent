// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-device-manager"

#include "config.h"

#include <gio/gio.h>

#include "valent-certificate.h"
#include "valent-channel.h"
#include "valent-channel-service.h"
#include "valent-component.h"
#include "valent-data.h"
#include "valent-debug.h"
#include "valent-device.h"
#include "valent-device-impl.h"
#include "valent-device-manager.h"
#include "valent-device-private.h"
#include "valent-global.h"
#include "valent-macros.h"
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
  GObject                   parent_instance;

  GCancellable             *cancellable;
  ValentData               *data;
  GTlsCertificate          *certificate;
  const char               *id;
  char                     *name;

  GHashTable               *devices;
  GHashTable               *services;
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

static void g_initable_iface_init       (GInitableIface      *iface);
static void g_async_initable_iface_init (GAsyncInitableIface *iface);

G_DEFINE_TYPE_WITH_CODE (ValentDeviceManager, valent_device_manager, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, g_initable_iface_init)
                         G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE, g_async_initable_iface_init))

enum {
  PROP_0,
  PROP_DATA,
  PROP_ID,
  PROP_NAME,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

enum {
  DEVICE_ADDED,
  DEVICE_REMOVED,
  N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0, };


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
typedef struct
{
  ValentDeviceManager *manager;
  PeasPluginInfo      *info;
  PeasExtension       *extension;
  GSettings           *settings;
} ChannelService;

static void
channel_service_free (gpointer data)
{
  ChannelService *info = data;

  if (info->extension)
    {
      g_signal_handlers_disconnect_by_data (info->extension, info->manager);
      valent_channel_service_stop (VALENT_CHANNEL_SERVICE (info->extension));
      g_clear_object (&info->extension);
    }

  g_clear_object (&info->settings);
  g_clear_pointer (&info, g_free);
}

static gboolean
valent_device_manager_check_device (ValentDeviceManager *self,
                                    ValentDevice        *device)
{
  GHashTableIter iter;
  gpointer value;
  unsigned int n_unpaired = 0;

  if (valent_device_get_paired (device))
    return TRUE;

  g_hash_table_iter_init (&iter, self->devices);

  while (g_hash_table_iter_next (&iter, NULL, &value))
    {
      if (!valent_device_get_paired (VALENT_DEVICE (value)))
        n_unpaired++;
    }

  return n_unpaired <= DEVICE_UNPAIRED_MAX;
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
      g_warning ("%s(): too many unpaired devices", G_STRFUNC);
      VALENT_EXIT;
    }

  valent_device_set_channel (device, channel);

  VALENT_EXIT;
}

static void
valent_channel_service_start_cb (ValentChannelService *service,
                                 GAsyncResult         *result,
                                 ValentDeviceManager  *self)
{
  g_autoptr (GError) error = NULL;

  VALENT_ENTRY;

  g_assert (VALENT_IS_CHANNEL_SERVICE (service));

  if (!valent_channel_service_start_finish (service, result, &error) &&
      !valent_error_ignore (error))
    g_warning ("%s: %s", G_OBJECT_TYPE_NAME (service), error->message);

  VALENT_EXIT;
}

static inline void
valent_device_manager_enable_service (ValentDeviceManager *self,
                                      ChannelService      *service)
{
  g_assert (VALENT_IS_DEVICE_MANAGER (self));
  g_assert (service != NULL);

  service->extension = peas_engine_create_extension (valent_get_plugin_engine (),
                                                     service->info,
                                                     VALENT_TYPE_CHANNEL_SERVICE,
                                                     "data", self->data,
                                                     "id",   self->id,
                                                     "name", self->name,
                                                     NULL);
  g_return_if_fail (PEAS_IS_EXTENSION (service->extension));

  g_object_bind_property (self,               "name",
                          service->extension, "name",
                          G_BINDING_DEFAULT);

  g_signal_connect (service->extension,
                    "channel",
                    G_CALLBACK (on_channel),
                    self);

  valent_channel_service_start (VALENT_CHANNEL_SERVICE (service->extension),
                                self->cancellable,
                                (GAsyncReadyCallback)valent_channel_service_start_cb,
                                self);
}

static inline void
valent_device_manager_disable_service (ValentDeviceManager *self,
                                       ChannelService      *service)
{
  g_assert (VALENT_IS_DEVICE_MANAGER (self));
  g_assert (service != NULL);
  g_return_if_fail (PEAS_IS_EXTENSION (service->extension));

  if (service->extension != NULL)
    {
      g_signal_handlers_disconnect_by_data (service->extension, self);
      valent_channel_service_stop (VALENT_CHANNEL_SERVICE (service->extension));
      g_clear_object (&service->extension);
    }
}

static inline void
valent_device_manager_identify_service (ValentDeviceManager *self,
                                        ChannelService      *service,
                                        const char          *target)
{
  g_assert (VALENT_IS_DEVICE_MANAGER (self));

  if (service->extension == NULL)
    return;

  valent_channel_service_identify (VALENT_CHANNEL_SERVICE (service->extension),
                                   target);
}

static void
on_enabled_changed (GSettings      *settings,
                    const char     *key,
                    ChannelService *service)
{
  ValentDeviceManager *self = service->manager;

  g_assert (G_IS_SETTINGS (settings));
  g_assert (VALENT_IS_DEVICE_MANAGER (self));

  if (g_settings_get_boolean (settings, key))
    valent_device_manager_enable_service (self, service);
  else
    valent_device_manager_disable_service (self, service);
}

static void
on_load_service (PeasEngine          *engine,
                 PeasPluginInfo      *info,
                 ValentDeviceManager *self)
{
  ChannelService *service;
  const char *module;

  g_assert (PEAS_IS_ENGINE (engine));
  g_assert (info != NULL);
  g_assert (VALENT_IS_DEVICE_MANAGER (self));

  /* We're only interested in one GType */
  if (!peas_engine_provides_extension (engine, info, VALENT_TYPE_CHANNEL_SERVICE))
    return;

  VALENT_NOTE ("%s: %s",
               g_type_name (VALENT_TYPE_CHANNEL_SERVICE),
               peas_plugin_info_get_module_name (info));

  module = peas_plugin_info_get_module_name (info);

  service = g_new0 (ChannelService, 1);
  service->manager = self;
  service->info = info;
  service->settings = valent_component_create_settings ("network", module);
  g_signal_connect (service->settings,
                    "changed::enabled",
                    G_CALLBACK (on_enabled_changed),
                    service);
  g_hash_table_insert (self->services, info, service);

  if (g_settings_get_boolean (service->settings, "enabled"))
    valent_device_manager_enable_service (self, service);
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

  g_hash_table_remove (self->services, info);
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

static void
valent_device_manager_add_device (ValentDeviceManager *self,
                                  ValentDevice        *device)
{
  const char *device_id;

  VALENT_ENTRY;

  g_assert (VALENT_IS_DEVICE_MANAGER (self));
  g_assert (VALENT_IS_DEVICE (device));

  device_id = valent_device_get_id (device);

  if (g_hash_table_contains (self->devices, device_id))
    VALENT_EXIT;

  g_signal_connect_object (device,
                           "notify::state",
                           G_CALLBACK (on_device_state),
                           self,
                           0);

  g_hash_table_insert (self->devices,
                       g_strdup (device_id),
                       g_object_ref (device));
  g_signal_emit (G_OBJECT (self), signals [DEVICE_ADDED], 0, device);

  if (self->dbus != NULL)
    valent_device_manager_export_device (self, device);

  VALENT_EXIT;
}

static void
valent_device_manager_remove_device (ValentDeviceManager *manager,
                                     ValentDevice        *device)
{
  VALENT_ENTRY;

  g_assert (VALENT_IS_DEVICE_MANAGER (manager));
  g_assert (VALENT_IS_DEVICE (device));

  g_object_ref (device);

  if (g_hash_table_remove (manager->devices, valent_device_get_id (device)))
    {
      valent_device_manager_unexport_device (manager, device);
      g_signal_handlers_disconnect_by_data (device, manager);
      g_signal_emit (G_OBJECT (manager), signals [DEVICE_REMOVED], 0, device);

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
      g_warning ("%s(): expected \"deviceId\" field holding a string",
                 G_STRFUNC);
      return NULL;
    }

  if (!g_hash_table_contains (manager->devices, device_id))
    {
      g_autoptr (ValentDevice) device = NULL;
      g_autoptr (ValentData) data = NULL;

      data = valent_data_new (device_id, manager->data);
      device = valent_device_new_full (identity, data);

      valent_device_manager_add_device (manager, device);
    }

  return g_hash_table_lookup (manager->devices, device_id);
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
      g_autofree char *path = NULL;

      path = g_build_filename (valent_data_get_cache_path (self->data),
                               "devices.json",
                               NULL);

      /* Try to load the state file */
      parser = json_parser_new ();

      if (json_parser_load_from_file (parser, path, NULL))
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
  g_autofree char *path = NULL;
  g_autoptr (GError) error = NULL;

  g_assert (VALENT_IS_DEVICE_MANAGER (self));

  generator = g_object_new (JSON_TYPE_GENERATOR,
                            "pretty", TRUE,
                            "root",   self->state,
                            NULL);

  path = g_build_filename (valent_data_get_cache_path (self->data),
                           "devices.json",
                           NULL);

  if (!json_generator_to_file (generator, path, &error))
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

  if (self->data == NULL)
    self->data = valent_data_new (NULL, NULL);

  /* Generate certificate */
  path = valent_data_get_config_path (self->data);
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
  const char *path = NULL;

  g_assert (VALENT_IS_DEVICE_MANAGER (self));

  /* Ensure we have a data manager */
  if (self->data == NULL)
    self->data = valent_data_new (NULL, NULL);

  task = g_task_new (initable, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_device_manager_init_async);
  g_task_set_priority (task, io_priority);

  path = valent_data_get_config_path (self->data);
  valent_certificate_new (path,
                          cancellable,
                          valent_certificate_new_cb,
                          g_steal_pointer (&task));
}

static gboolean
valent_device_manager_init_finish (GAsyncInitable  *initable,
                                   GAsyncResult    *result,
                                   GError         **error)
{
  g_return_val_if_fail (g_task_is_valid (result, initable), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
g_async_initable_iface_init (GAsyncInitableIface *iface)
{
  iface->init_async = valent_device_manager_init_async;
  iface->init_finish = valent_device_manager_init_finish;
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
  g_clear_pointer (&self->services, g_hash_table_unref);
  g_clear_pointer (&self->devices, g_hash_table_unref);
  g_clear_pointer (&self->state, json_node_unref);

  g_clear_object (&self->certificate);
  g_clear_object (&self->data);
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
    case PROP_DATA:
      g_value_set_object (value, self->data);
      break;

    case PROP_ID:
      g_value_set_string (value, self->id);
      break;

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
    case PROP_DATA:
      self->data = g_value_dup_object (value);
      break;

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
   * ValentDeviceManager:data:
   *
   * The data context.
   *
   * Usually this is the root data context for the application.
   *
   * Since: 1.0
   */
  properties [PROP_DATA] =
    g_param_spec_object ("data", NULL, NULL,
                         VALENT_TYPE_DATA,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentDeviceManager:id: (getter get_id)
   *
   * The unique ID of the local device.
   *
   * This is intended to be unique within the device's network ok KDE Connect
   * clients. The ID is equivalent to the common name of the service TLS
   * certificate, which will be generated if necessary.
   *
   * Since: 1.0
   */
  properties [PROP_ID] =
    g_param_spec_string ("id", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

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

  /**
   * ValentDeviceManager::device-added:
   * @manager: a #ValentDeviceManager
   * @device: a #ValentDevice
   *
   * Emitted when a new [class@Valent.Device] has been added to @manager.
   *
   * Since: 1.0
   */
  signals [DEVICE_ADDED] =
    g_signal_new ("device-added",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, VALENT_TYPE_DEVICE);
  g_signal_set_va_marshaller (signals [DEVICE_ADDED],
                              G_TYPE_FROM_CLASS (klass),
                              g_cclosure_marshal_VOID__OBJECTv);

  /**
   * ValentDeviceManager::device-removed:
   * @manager: a #ValentDeviceManager
   * @device: a #ValentDevice
   *
   * Emitted when a [class@Valent.Device] has been removed from @manager.
   *
   * Devices are removed automatically when they become both unpaired and
   * disconnected.
   *
   * Since: 1.0
   */
  signals [DEVICE_REMOVED] =
    g_signal_new ("device-removed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, VALENT_TYPE_DEVICE);
  g_signal_set_va_marshaller (signals [DEVICE_REMOVED],
                              G_TYPE_FROM_CLASS (klass),
                              g_cclosure_marshal_VOID__OBJECTv);
}

static void
valent_device_manager_init (ValentDeviceManager *self)
{
  self->devices = g_hash_table_new_full (g_str_hash,
                                         g_str_equal,
                                         g_free,
                                         g_object_unref);
  self->services = g_hash_table_new_full (NULL,
                                          NULL,
                                          NULL,
                                          channel_service_free);
  self->exported = g_hash_table_new (NULL, NULL);
}

/**
 * valent_device_manager_new_sync:
 * @data: (nullable): a #ValentData
 * @cancellable: (nullable): a #GCancellable
 * @error: (nullable): a #GError
 *
 * Create a new #ValentDeviceManager.
 *
 * If given, @data will be used as the root [class@Valent.Data] for all
 * [class@Valent.ChannelService] implementations and [class@Valent.Device]
 * instances.
 *
 * Returns: (transfer full) (nullable): a #ValentDeviceManager
 *
 * Since: 1.0
 */
ValentDeviceManager *
valent_device_manager_new_sync (ValentData    *data,
                                GCancellable  *cancellable,
                                GError       **error)
{
  GInitable *manager;

  g_return_val_if_fail (data == NULL || VALENT_IS_DATA (data), NULL);
  g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  manager = g_initable_new (VALENT_TYPE_DEVICE_MANAGER,
                            cancellable,
                            error,
                            "data", data,
                            NULL);

  if (manager == NULL)
    return NULL;

  return VALENT_DEVICE_MANAGER (manager);
}

/**
 * valent_device_manager_new:
 * @data: (nullable): a #ValentData
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): a #GAsyncReadyCallback
 * @user_data: (closure): user supplied data
 *
 * Asynchronously create a new #ValentDeviceManager.
 *
 * If given, @data will be used as the root [class@Valent.Data] for all
 * [class@Valent.ChannelService] implementations and [class@Valent.Device]
 * instances.
 *
 * When the manager is ready @callback will be invoked and you can use
 * [ctor@Valent.DeviceManager.new_finish] to get the result.
 *
 * Since: 1.0
 */
void
valent_device_manager_new (ValentData          *data,
                           GCancellable        *cancellable,
                           GAsyncReadyCallback  callback,
                           gpointer             user_data)
{
  g_return_if_fail (data == NULL || VALENT_IS_DATA (data));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  g_async_initable_new_async (VALENT_TYPE_DEVICE_MANAGER,
                              G_PRIORITY_DEFAULT,
                              cancellable,
                              callback,
                              user_data,
                              "data", data,
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
 * valent_device_manager_get_device:
 * @manager: a #ValentDeviceManager
 * @id: the id of the device
 *
 * Try to find a #ValentDevice with the id @id, otherwise return %NULL.
 *
 * Returns: (transfer none) (nullable): a #ValentDevice
 *
 * Since: 1.0
 */
ValentDevice *
valent_device_manager_get_device (ValentDeviceManager *manager,
                                  const char          *id)
{
  g_return_val_if_fail (VALENT_IS_DEVICE_MANAGER (manager), NULL);
  g_return_val_if_fail (id != NULL, NULL);

  return g_hash_table_lookup (manager->devices, id);
}

/**
 * valent_device_manager_get_devices:
 * @manager: a #ValentDeviceManager
 *
 * Get a list of the the [class@Valent.Device] objects managed by @manager.
 *
 * Returns: (transfer container) (element-type Valent.Device): a #GPtrArray
 *
 * Since: 1.0
 */
GPtrArray *
valent_device_manager_get_devices (ValentDeviceManager *manager)
{
  g_autoptr (GPtrArray) devices = NULL;
  GHashTableIter iter;
  ValentDevice *device;

  g_return_val_if_fail (VALENT_IS_DEVICE_MANAGER (manager), NULL);

  devices = g_ptr_array_new_with_free_func (g_object_unref);

  g_hash_table_iter_init (&iter, manager->devices);

  while (g_hash_table_iter_next (&iter, NULL, (void **)&device))
    g_ptr_array_add (devices, g_object_ref (device));

  return g_steal_pointer (&devices);
}

/**
 * valent_device_manager_get_id: (get-property id)
 * @manager: a #ValentDeviceManager
 *
 * Get a copy of the identity string for this device manager.
 *
 * Returns: (transfer none): the identity string
 */
const char *
valent_device_manager_get_id (ValentDeviceManager *manager)
{
  g_return_val_if_fail (VALENT_IS_DEVICE_MANAGER (manager), NULL);

  return manager->id;
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

  if (g_strcmp0 (manager->name, name) == 0)
    return;

  g_clear_pointer (&manager->name, g_free);
  manager->name = g_strdup (name);
  g_object_notify_by_pspec (G_OBJECT (manager), properties [PROP_NAME]);
}

/**
 * valent_device_manager_identify:
 * @manager: a #ValentDeviceManager
 * @uri: a URI
 *
 * Identify the local device to the network.
 *
 * This method calls [method@Valent.ChannelService.identify] for each loaded
 * [class@Valent.ChannelService], requesting to identify itself on its
 * respective network.
 *
 * The @uri argument is string in the form `plugin://address`, such as
 * `lan://192.168.0.10:1716`. The `plugin` segment should be the module name of
 * a plugin that implements [class@Valent.ChannelService] and `address` segment
 * should be a format the implementation understands.
 *
 * Since: 1.0
 */
void
valent_device_manager_identify (ValentDeviceManager *manager,
                                const char          *uri)
{
  GHashTableIter iter;
  gpointer info, service;

  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_DEVICE_MANAGER (manager));

  if (uri != NULL)
    {
      g_auto (GStrv) address = NULL;

      address = g_strsplit (uri, "://", -1);

      if (address[0] == NULL || address[1] == NULL)
        VALENT_EXIT;

      g_hash_table_iter_init (&iter, manager->services);

      while (g_hash_table_iter_next (&iter, &info, &service))
        {
          const char *module = peas_plugin_info_get_module_name (info);

          if (g_str_equal (address[0], module))
            valent_device_manager_identify_service (manager, service, address[1]);
        }
    }
  else
    {
      g_hash_table_iter_init (&iter, manager->services);

      while (g_hash_table_iter_next (&iter, NULL, &service))
        valent_device_manager_identify_service (manager, service, NULL);
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
  GHashTableIter iter;
  g_autofree char *device_id = NULL;
  g_autoptr (ValentDevice) device = NULL;

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
  g_hash_table_remove_all (manager->services);

  /* Remove any devices */
  g_hash_table_iter_init (&iter, manager->devices);

  while (g_hash_table_iter_next (&iter, (void **)&device_id, (void **)&device))
    {
      g_hash_table_iter_steal (&iter);
      g_signal_handlers_disconnect_by_data (device, manager);
      g_signal_emit (G_OBJECT (manager), signals [DEVICE_REMOVED], 0, device);

      g_clear_pointer (&device_id, g_free);
      g_clear_object (&device);
    }

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
  GHashTableIter iter;
  ValentDevice *device;

  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_DEVICE_MANAGER (manager));
  g_return_if_fail (G_IS_DBUS_CONNECTION (connection));
  g_return_if_fail (g_variant_is_object_path (object_path));

  if (manager->dbus != NULL)
    VALENT_EXIT;

  manager->dbus = g_dbus_object_manager_server_new (object_path);
  g_dbus_object_manager_server_set_connection (manager->dbus, connection);

  g_hash_table_iter_init (&iter, manager->devices);

  while (g_hash_table_iter_next (&iter, NULL, (void **)&device))
    valent_device_manager_export_device (manager, device);

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
  GHashTableIter iter;
  ValentDevice *device;

  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_DEVICE_MANAGER (manager));

  if (manager->dbus == NULL)
    VALENT_EXIT;

  g_hash_table_iter_init (&iter, manager->devices);

  while (g_hash_table_iter_next (&iter, NULL, (void **)&device))
    valent_device_manager_unexport_device (manager, device);

  g_dbus_object_manager_server_set_connection (manager->dbus, NULL);
  g_clear_object (&manager->dbus);

  VALENT_EXIT;
}

