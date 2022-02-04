// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-manager"

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
#include "valent-device-private.h"
#include "valent-macros.h"
#include "valent-manager.h"
#include "valent-packet.h"
#include "valent-utils.h"


/**
 * SECTION:valentmanager
 * @short_description: Controls an instance of Valent
 * @title: ValentManager
 * @stability: Unstable
 * @include: libvalent-core.h
 *
 * #ValentManager effectively represents an instance of Valent, including the
 * available #ValentChannelService implementations that provide #ValentChannel
 * instances passed to #ValentDevice instances.
 */

struct _ValentManager
{
  GObject                   parent_instance;

  GCancellable             *cancellable;
  ValentData               *data;
  GTlsCertificate          *certificate;
  const char               *id;
  GSettings                *settings;

  PeasEngine               *engine;
  GHashTable               *devices;
  GHashTable               *services;

  GDBusObjectManagerServer *dbus;
  GHashTable               *exported;
};

static void           valent_manager_add_device    (ValentManager *manager,
                                                    ValentDevice  *device);
static void           valent_manager_remove_device (ValentManager *manager,
                                                    ValentDevice  *device);
static ValentDevice * valent_manager_ensure_device (ValentManager *manager,
                                                    JsonNode      *identity);

static void initable_iface_init       (GInitableIface      *iface);
static void async_initable_iface_init (GAsyncInitableIface *iface);

G_DEFINE_TYPE_WITH_CODE (ValentManager, valent_manager, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, initable_iface_init)
                         G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE, async_initable_iface_init))

enum {
  PROP_0,
  PROP_DATA,
  PROP_ID,
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
get_device_dbus_path (ValentDevice *device)
{
  unsigned int len = 0;
  const char *base_path;
  const char *id;
  char object_path[256] = { 0, };

  g_assert (VALENT_IS_DEVICE (device));

  base_path = APPLICATION_PATH"/Device/";

  while (*base_path && len < 255)
    {
      object_path[len++] = *base_path;
      base_path++;
    }

  id = valent_device_get_id (device);

  while (*id && len < 255)
    {
      if G_LIKELY (g_ascii_isalnum (*id))
        object_path[len++] = *id;
      else
        object_path[len++] = '_';

      id++;
    }

  return g_strdup (object_path);
}

static void
valent_manager_export_device (ValentManager *manager,
                              ValentDevice  *device)
{
  g_autoptr (GDBusObjectSkeleton) object = NULL;
  g_autoptr (GDBusInterfaceSkeleton) iface = NULL;
  ExportedDevice *info;
  GActionGroup *actions;
  GMenuModel *menu;

  VALENT_ENTRY;

  g_assert (VALENT_IS_MANAGER (manager));
  g_assert (VALENT_IS_DEVICE (device));

  if (g_hash_table_contains (manager->exported, device))
    VALENT_EXIT;

  info = g_new0 (ExportedDevice, 1);
  info->connection = g_dbus_object_manager_server_get_connection (manager->dbus);
  info->object_path = get_device_dbus_path (device);

  /* Export the ValentDevice, GActionGroup and GMenuModel interfaces on the same
   * connection and path */
  object = g_dbus_object_skeleton_new (info->object_path);
  iface = valent_device_impl_new (device);
  g_dbus_object_skeleton_add_interface (object, iface);

  actions = valent_device_get_actions (device);
  info->actions_id = g_dbus_connection_export_action_group (info->connection,
                                                            info->object_path,
                                                            actions,
                                                            NULL);

  menu = valent_device_get_menu (device);
  info->menu_id = g_dbus_connection_export_menu_model (info->connection,
                                                       info->object_path,
                                                       menu,
                                                       NULL);

  g_dbus_object_manager_server_export (manager->dbus, object);
  g_hash_table_insert (manager->exported, device, info);

  VALENT_EXIT;
}

static void
valent_manager_unexport_device (ValentManager *manager,
                                ValentDevice  *device)
{
  gpointer data;
  ExportedDevice *info = NULL;

  VALENT_ENTRY;

  g_assert (VALENT_IS_MANAGER (manager));
  g_assert (VALENT_IS_DEVICE (device));

  if (!g_hash_table_steal_extended (manager->exported, device, NULL, &data))
    VALENT_EXIT;

  info = (ExportedDevice *)data;

  g_dbus_object_manager_server_unexport (manager->dbus, info->object_path);
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
  ValentManager  *manager;
  PeasPluginInfo *info;
  PeasExtension  *extension;
  GSettings      *settings;
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

static void
on_channel (ValentChannelService *service,
            ValentChannel        *channel,
            ValentManager        *manager)
{
  JsonNode *identity;
  ValentDevice *device;

  VALENT_ENTRY;

  g_assert (VALENT_IS_CHANNEL_SERVICE (service));
  g_assert (VALENT_IS_CHANNEL (channel));
  g_assert (VALENT_IS_MANAGER (manager));

  if ((identity = valent_channel_get_peer_identity (channel)) == NULL)
    {
      g_warning ("%s(): %s missing peer identity",
                 G_STRFUNC,
                 G_OBJECT_TYPE_NAME (channel));
      VALENT_EXIT;
    }

  if ((device = valent_manager_ensure_device (manager, identity)) != NULL)
    valent_device_set_channel (device, channel);

  VALENT_EXIT;
}

static void
valent_channel_service_start_cb (ValentChannelService *service,
                                 GAsyncResult         *result,
                                 ValentManager        *manager)
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
valent_manager_enable_service (ValentManager  *self,
                               ChannelService *service)
{
  g_assert (VALENT_IS_MANAGER (self));
  g_assert (service != NULL);

  service->extension = peas_engine_create_extension (self->engine,
                                                     service->info,
                                                     VALENT_TYPE_CHANNEL_SERVICE,
                                                     "data", self->data,
                                                     "id",   self->id,
                                                     NULL);
  g_return_if_fail (PEAS_IS_EXTENSION (service->extension));

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
valent_manager_disable_service (ValentManager  *self,
                                ChannelService *service)
{
  g_assert (VALENT_IS_MANAGER (self));
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
valent_manager_identify_service (ValentManager  *self,
                                 ChannelService *service,
                                 const char     *target)
{
  g_assert (VALENT_IS_MANAGER (self));

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
  ValentManager *self = service->manager;

  g_assert (G_IS_SETTINGS (settings));
  g_assert (VALENT_IS_MANAGER (self));

  if (g_settings_get_boolean (settings, key))
    valent_manager_enable_service (self, service);
  else
    valent_manager_disable_service (self, service);
}

static void
on_load_service (PeasEngine     *engine,
                 PeasPluginInfo *info,
                 ValentManager  *self)
{
  ChannelService *service;
  const char *module;

  g_assert (PEAS_IS_ENGINE (engine));
  g_assert (info != NULL);
  g_assert (VALENT_IS_MANAGER (self));

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
  service->settings = valent_component_new_settings ("network", module);
  g_signal_connect (service->settings,
                    "changed::enabled",
                    G_CALLBACK (on_enabled_changed),
                    service);
  g_hash_table_insert (self->services, info, service);

  if (g_settings_get_boolean (service->settings, "enabled"))
    valent_manager_enable_service (self, service);
}

static void
on_unload_service (PeasEngine     *engine,
                   PeasPluginInfo *info,
                   ValentManager  *self)
{
  g_assert (PEAS_IS_ENGINE (engine));
  g_assert (info != NULL);
  g_assert (VALENT_IS_MANAGER (self));

  /* We're only interested in one GType */
  if (!peas_engine_provides_extension (engine, info, VALENT_TYPE_CHANNEL_SERVICE))
    return;

  VALENT_NOTE ("%s: %s",
               g_type_name (VALENT_TYPE_CHANNEL_SERVICE),
               peas_plugin_info_get_module_name (info));

  g_hash_table_remove (self->services, info);
}


/*
 * Cached Devices
 */
typedef struct
{
  GWeakRef  manager;
  JsonNode *identity;
} CachedDevice;

static void
cached_device_free (gpointer data)
{
  CachedDevice *cache = data;

  g_weak_ref_clear (&cache->manager);
  g_clear_pointer (&cache->identity, json_node_unref);
  g_free (cache);
}

static gboolean
ensure_device_main (gpointer user_data)
{
  CachedDevice *data = user_data;
  g_autoptr (ValentManager) manager = NULL;

  g_assert (VALENT_IS_MAIN_THREAD ());

  if ((manager = g_weak_ref_get (&data->manager)) != NULL)
    valent_manager_ensure_device (manager, data->identity);

  return G_SOURCE_REMOVE;
}

static gboolean
valent_manager_load_devices (ValentManager  *self,
                             GFile          *file,
                             GCancellable   *cancellable,
                             GError        **error)
{
  g_autoptr (GFileEnumerator) iter = NULL;
  g_autoptr (JsonParser) parser = NULL;
  GFile *child = NULL;

  /* Look in the config directory for subdirectories. We only fail on
   * G_IO_ERROR_CANCELLED; all other errors mean there are just no devices. */
  iter = g_file_enumerate_children (file,
                                    G_FILE_ATTRIBUTE_STANDARD_NAME,
                                    G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                    cancellable,
                                    error);

  if (iter == NULL)
    {
      if (g_cancellable_is_cancelled (cancellable))
        return FALSE;

      g_clear_error (error);
      return TRUE;
    }

  /* Iterate the subdirectories looking for identity.json files */
  parser = json_parser_new ();

  while (g_file_enumerator_iterate (iter, NULL, &child, cancellable, error))
    {
      g_autoptr (GFile) identity_json = NULL;

      if (child == NULL)
        break;

      /* Check if identity.json exists */
      identity_json = g_file_get_child (child, "identity.json");

      if (g_file_query_exists (identity_json, cancellable))
        {
          g_autoptr (JsonNode) packet = NULL;
          g_autoptr (GError) warning = NULL;
          const char *path;
          CachedDevice *cached_device;

          path = g_file_peek_path (identity_json);

          if (!json_parser_load_from_file (parser, path, &warning))
            {
              g_warning ("%s(): failed to parse \"%s\": %s",
                         G_STRFUNC,
                         path,
                         warning->message);
              continue;
            }

          packet = json_parser_steal_root (parser);

          if (!valent_packet_validate (packet, &warning))
            {
              g_warning ("%s(): failed to validate \"%s\": %s",
                         G_STRFUNC,
                         path,
                         warning->message);
              continue;
            }

          /* The ValentDevice has to be constructed in the main thread */
          cached_device = g_new0 (CachedDevice, 1);
          g_weak_ref_init (&cached_device->manager, self);
          cached_device->identity = g_steal_pointer (&packet);

          g_main_context_invoke_full (NULL,
                                      G_PRIORITY_DEFAULT,
                                      ensure_device_main,
                                      cached_device,
                                      cached_device_free);
        }
      else if (g_cancellable_is_cancelled (cancellable))
        return FALSE;
    }

  if (g_cancellable_is_cancelled (cancellable))
    return FALSE;

  g_clear_error (error);

  return TRUE;
}

/*
 * Device Management
 */
static gboolean
valent_manager_remove_device_main (gpointer data)
{
  g_object_unref (VALENT_DEVICE (data));

  return G_SOURCE_REMOVE;
}

static void
on_device_state (ValentDevice  *device,
                 GParamSpec    *pspec,
                 ValentManager *manager)
{
  ValentDeviceState state = valent_device_get_state (device);

  if ((state & VALENT_DEVICE_STATE_CONNECTED) != 0 ||
      (state & VALENT_DEVICE_STATE_PAIRED) != 0)
    return;

  valent_manager_remove_device (manager, device);
}

static void
valent_manager_add_device (ValentManager *manager,
                           ValentDevice  *device)
{
  const char *device_id;

  VALENT_ENTRY;

  g_assert (VALENT_IS_MANAGER (manager));
  g_assert (VALENT_IS_DEVICE (device));

  device_id = valent_device_get_id (device);

  if (g_hash_table_contains (manager->devices, device_id))
    VALENT_EXIT;

  g_signal_connect_object (device,
                           "notify::state",
                           G_CALLBACK (on_device_state),
                           manager,
                           0);

  g_hash_table_insert (manager->devices,
                       g_strdup (device_id),
                       g_object_ref (device));
  g_signal_emit (G_OBJECT (manager), signals [DEVICE_ADDED], 0, device);

  if (manager->dbus != NULL)
    valent_manager_export_device (manager, device);

  VALENT_EXIT;
}

static void
valent_manager_remove_device (ValentManager *manager,
                              ValentDevice  *device)
{
  VALENT_ENTRY;

  g_assert (VALENT_IS_MANAGER (manager));
  g_assert (VALENT_IS_DEVICE (device));

  g_object_ref (device);

  if (g_hash_table_remove (manager->devices, valent_device_get_id (device)))
    {
      valent_manager_unexport_device (manager, device);
      g_signal_handlers_disconnect_by_data (device, manager);
      g_signal_emit (G_OBJECT (manager), signals [DEVICE_REMOVED], 0, device);

      // HACK: we are in a signal handler of a device's `notify::state`
      //       emission, so if we drop the last reference the emitting object
      //       and other handlers may be setup for a use-after-free error.
      g_idle_add (valent_manager_remove_device_main, g_object_ref (device));
    }

  g_object_unref (device);

  VALENT_EXIT;
}

static ValentDevice *
valent_manager_ensure_device (ValentManager *manager,
                              JsonNode      *identity)
{
  ValentDevice *device = NULL;
  const char *device_id;

  g_assert (VALENT_IS_MANAGER (manager));
  g_assert (VALENT_IS_PACKET (identity));

  if (!valent_packet_get_string (identity, "deviceId", &device_id))
    {
      g_warning ("%s(): expected \"deviceId\" field holding a string",
                 G_STRFUNC);
      return NULL;
    }

  device = g_hash_table_lookup (manager->devices, device_id);

  if (device == NULL)
    {
      g_autoptr (ValentData) data = NULL;

      data = valent_data_new (device_id, manager->data);
      device = g_object_new (VALENT_TYPE_DEVICE,
                             "id",   device_id,
                             "data", data,
                             NULL);
      valent_device_handle_packet (device, identity);

      valent_manager_add_device (manager, device);
      g_object_unref (device);
    }

  return device;
}

/*
 * GInitable
 */
static gboolean
valent_manager_initable_init (GInitable     *initable,
                              GCancellable  *cancellable,
                              GError       **error)
{
  ValentManager *self = VALENT_MANAGER (initable);
  g_autoptr (GFile) file = NULL;
  const char *path = NULL;

  if (self->data == NULL)
    self->data = valent_data_new (NULL, NULL);

  path = valent_data_get_config_path (self->data);
  file = g_file_new_for_path (path);

  /* Generate certificate */
  self->certificate = valent_certificate_new_sync (path, error);

  if (self->certificate == NULL)
    return FALSE;

  self->id = valent_certificate_get_common_name (self->certificate);

  /* Load devices */
  if (!valent_manager_load_devices (self, file, cancellable, error))
    return FALSE;

  return TRUE;
}

static void
initable_iface_init (GInitableIface *iface)
{
  iface->init = valent_manager_initable_init;
}

/*
 * GAsyncInitable
 */
static void
load_devices_task (GTask        *task,
                   gpointer      source_object,
                   gpointer      task_data,
                   GCancellable *cancellable)
{
  ValentManager *self = VALENT_MANAGER (source_object);
  GFile *file = G_FILE (task_data);
  GError *error = NULL;

  if (!valent_manager_load_devices (self, file, cancellable, &error))
    return g_task_return_error (task, error);

  g_task_return_boolean (task, TRUE);
}

static void
valent_certificate_new_cb (GObject      *object,
                           GAsyncResult *result,
                           gpointer      user_data)
{
  g_autoptr (GTask) task = G_TASK (user_data);
  ValentManager *self = g_task_get_source_object (task);
  GError *error = NULL;

  self->certificate = valent_certificate_new_finish (result, &error);

  if (error != NULL)
    return g_task_return_error (task, error);

  self->id = valent_certificate_get_common_name (self->certificate);

  g_task_run_in_thread (task, load_devices_task);
}

static void
valent_manager_init_async (GAsyncInitable      *initable,
                           int                  io_priority,
                           GCancellable        *cancellable,
                           GAsyncReadyCallback  callback,
                           gpointer             user_data)
{
  ValentManager *self = VALENT_MANAGER (initable);
  g_autoptr (GTask) task = NULL;
  g_autoptr (GFile) file = NULL;
  const char *path = NULL;

  g_assert (VALENT_IS_MANAGER (self));

  /* Ensure we have a data manager */
  if (self->data == NULL)
    self->data = valent_data_new (NULL, NULL);

  path = valent_data_get_config_path (self->data);
  file = g_file_new_for_path (path);

  task = g_task_new (initable, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_manager_init_async);
  g_task_set_task_data (task, g_steal_pointer (&file), g_object_unref);
  g_task_set_priority (task, io_priority);

  valent_certificate_new (path,
                          cancellable,
                          valent_certificate_new_cb,
                          g_steal_pointer (&task));
}

static gboolean
valent_manager_init_finish (GAsyncInitable  *initable,
                            GAsyncResult    *result,
                            GError         **error)
{
  g_return_val_if_fail (g_task_is_valid (result, initable), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
async_initable_iface_init (GAsyncInitableIface *iface)
{
  iface->init_async = valent_manager_init_async;
  iface->init_finish = valent_manager_init_finish;
}

/*
 * GObject
 */
static void
valent_manager_dispose (GObject *object)
{
  ValentManager *self = VALENT_MANAGER (object);
  GHashTableIter iter;
  ValentDevice *device;

  valent_manager_stop (self);
  valent_manager_unexport (self);

  g_hash_table_iter_init (&iter, self->devices);

  while (g_hash_table_iter_next (&iter, NULL, (void **)&device))
    {
      g_signal_handlers_disconnect_by_data (device, self);
      g_signal_emit (G_OBJECT (self), signals [DEVICE_REMOVED], 0, device);
      g_hash_table_iter_remove (&iter);
    }
}

static void
valent_manager_finalize (GObject *object)
{
  ValentManager *self = VALENT_MANAGER (object);

  g_clear_pointer (&self->exported, g_hash_table_unref);
  g_clear_pointer (&self->services, g_hash_table_unref);
  g_clear_pointer (&self->devices, g_hash_table_unref);

  g_clear_object (&self->certificate);
  g_clear_object (&self->data);
  g_clear_object (&self->settings);

  G_OBJECT_CLASS (valent_manager_parent_class)->finalize (object);
}

static void
valent_manager_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  ValentManager *self = VALENT_MANAGER (object);

  switch (prop_id)
    {
    case PROP_DATA:
      g_value_set_object (value, self->data);
      break;

    case PROP_ID:
      g_value_set_string (value, self->id);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_manager_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  ValentManager *self = VALENT_MANAGER (object);

  switch (prop_id)
    {
    case PROP_DATA:
      self->data = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_manager_class_init (ValentManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = valent_manager_dispose;
  object_class->finalize = valent_manager_finalize;
  object_class->get_property = valent_manager_get_property;
  object_class->set_property = valent_manager_set_property;

  /**
   * ValentManager:data:
   *
   * The #ValentData for the manager.
   */
  properties [PROP_DATA] =
    g_param_spec_object ("data",
                         "Data Manager",
                         "The data manager for this manager",
                         VALENT_TYPE_DATA,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentManager:id:
   *
   * The unique ID this device will identify as.
   *
   * The ID is equivalent to the common name of the service TLS certificate,
   * which will generated if necessary.
   */
  properties [PROP_ID] =
    g_param_spec_string ("id",
                         "Id",
                         "A unique Id",
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);

  /**
   * ValentManager::device-added:
   * @manager: a #ValentManager
   * @device: a #ValentDevice
   *
   * #ValentManager::device-added is emitted when a new #ValentDevice has
   * been added to @manager. This usually happens when a #ValentChannelService
   * emits #ValentChannelService::channel with an unknown identity.
   *
   * Note that the internal state of @manager has already been updated when this
   * signal is emitted and @manager will hold a reference to @device.
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
   * ValentManager::device-removed:
   * @manager: a #ValentManager
   * @device: a #ValentDevice
   *
   * #ValentManager::device-removed is emitted when a #ValentDevice has
   * been removed from @manager. This usually happens when a device becomes
   * both disconnected and unpaired, but also happens during shutdown when
   * valent_manager_stop() is called.
   *
   * Note that the internal state of @manager has already be updated when this
   * signal is emitted, but the last reference to @device won't be dropped until
   * the last signal handler returns.
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
valent_manager_init (ValentManager *self)
{
  self->data = NULL;
  self->engine = valent_get_engine ();
  self->id = NULL;
  self->settings = g_settings_new ("ca.andyholmes.Valent");

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
 * valent_manager_new_sync:
 * @data: (nullable): a #ValentData
 * @cancellable: (nullable): a #GCancellable
 * @error: (nullable): a #GError
 *
 * Create a new #ValentManager.
 *
 * If given, @data will be used as the root #ValentData for this instance of
 * Valent.
 *
 * Returns: (transfer full) (nullable): a #ValentManager
 */
ValentManager *
valent_manager_new_sync (ValentData    *data,
                         GCancellable  *cancellable,
                         GError       **error)
{
  GInitable *manager;

  g_return_val_if_fail (data == NULL || VALENT_IS_DATA (data), NULL);
  g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  manager = g_initable_new (VALENT_TYPE_MANAGER,
                            cancellable,
                            error,
                            "data", data,
                            NULL);

  if (manager == NULL)
    return NULL;

  return VALENT_MANAGER (manager);
}

/**
 * valent_manager_new:
 * @data: (nullable): a #ValentData
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): a #GAsyncReadyCallback
 * @user_data: (closure): user supplied data
 *
 * Create a new #ValentManager.
 *
 * This is a failable asynchronous constructor - when the manager is ready,
 * @callback will be invoked and you can use valent_manager_new_finish() to get
 * the result.
 */
void
valent_manager_new (ValentData          *data,
                    GCancellable        *cancellable,
                    GAsyncReadyCallback  callback,
                    gpointer             user_data)
{
  g_return_if_fail (data == NULL || VALENT_IS_DATA (data));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  g_async_initable_new_async (VALENT_TYPE_MANAGER,
                              G_PRIORITY_DEFAULT,
                              cancellable,
                              callback,
                              user_data,
                              "data", data,
                              NULL);
}

/**
 * valent_manager_new_finish:
 * @result: a #GAsyncResult
 * @error: (nullable): a #GError
 *
 * Finish an operation started by valent_manager_new().
 *
 * Returns: (transfer full) (nullable): a #ValentManager
 */
ValentManager *
valent_manager_new_finish (GAsyncResult  *result,
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

  return VALENT_MANAGER (manager);
}

/**
 * valent_manager_get_device:
 * @manager: a #ValentManager
 * @id: the id of the device
 *
 * Try to find a #ValentDevice with the id @id, otherwise return %NULL.
 *
 * Returns: (transfer none) (nullable): a #ValentDevice
 */
ValentDevice *
valent_manager_get_device (ValentManager *manager,
                           const char    *id)
{
  g_return_val_if_fail (VALENT_IS_MANAGER (manager), NULL);
  g_return_val_if_fail (id != NULL, NULL);

  return g_hash_table_lookup (manager->devices, id);
}

/**
 * valent_manager_get_devices:
 * @manager: a #ValentManager
 *
 * Get a list of the the devices being managed by @manager.
 *
 * Returns: (transfer full) (element-type Valent.Device): a #GPtrArray
 */
GPtrArray *
valent_manager_get_devices (ValentManager *manager)
{
  g_autoptr (GPtrArray) devices = NULL;
  GHashTableIter iter;
  ValentDevice *device;

  g_return_val_if_fail (VALENT_IS_MANAGER (manager), NULL);

  devices = g_ptr_array_new_with_free_func (g_object_unref);

  g_hash_table_iter_init (&iter, manager->devices);

  while (g_hash_table_iter_next (&iter, NULL, (void **)&device))
    g_ptr_array_add (devices, g_object_ref (device));

  return g_steal_pointer (&devices);
}

/**
 * valent_manager_get_id:
 * @manager: a #ValentManager
 *
 * Get a copy of the identity string for this device manager.
 *
 * Returns: (transfer none): the identity string
 */
const char *
valent_manager_get_id (ValentManager *manager)
{
  g_return_val_if_fail (VALENT_IS_MANAGER (manager), NULL);

  return manager->id;
}

/**
 * valent_manager_identify:
 * @manager: a #ValentManager
 * @uri: a URI
 *
 * Request a connection from the device at @uri if given, otherwise ask each
 * loaded #ValentChannelService to identity itself on its respective network.
 *
 * The @uri argument is string in the form `plugin://address`, such as
 * `lan://192.168.0.10:1716`. The `plugin` segment should be a module name and
 * `address` should be a format the #ValentChannelService understands. Typically
 * these URIs are acquired from the #ValentChannel:uri property.
 */
void
valent_manager_identify (ValentManager *manager,
                         const char    *uri)
{
  GHashTableIter iter;
  gpointer info, service;

  g_return_if_fail (VALENT_IS_MANAGER (manager));

  if (uri != NULL)
    {
      g_auto (GStrv) address = NULL;

      address = g_strsplit (uri, "://", -1);

      if (address[0] == NULL || address[1] == NULL)
        return;

      g_hash_table_iter_init (&iter, manager->services);

      while (g_hash_table_iter_next (&iter, &info, &service))
        {
          const char *module = peas_plugin_info_get_module_name (info);

          if (g_str_equal (address[0], module))
            valent_manager_identify_service (manager, service, address[1]);
        }
    }
  else
    {
      g_hash_table_iter_init (&iter, manager->services);

      while (g_hash_table_iter_next (&iter, NULL, &service))
        valent_manager_identify_service (manager, service, NULL);
    }
}

/**
 * valent_manager_start:
 * @manager: a #ValentManager
 *
 * Load all the #ValentChannelService implementations known to the #ValentEngine,
 * thereby allowing new connections to be opened.
 */
void
valent_manager_start (ValentManager *manager)
{
  const GList *plugins = NULL;

  g_return_if_fail (VALENT_IS_MANAGER (manager));

  /* We're already started */
  if (manager->cancellable != NULL)
    return;

  /* Setup services */
  manager->cancellable = g_cancellable_new ();
  plugins = peas_engine_get_plugin_list (manager->engine);

  for (const GList *iter = plugins; iter; iter = iter->next)
    on_load_service (manager->engine, iter->data, manager);

  g_signal_connect_after (manager->engine,
                          "load-plugin",
                          G_CALLBACK (on_load_service),
                          manager);
  g_signal_connect (manager->engine,
                    "unload-plugin",
                    G_CALLBACK (on_unload_service),
                    manager);
}

/**
 * valent_manager_stop:
 * @manager: a #ValentManager
 *
 * Unload all the #ValentChannelService implementations loaded from the
 * #ValentEngine, thereby preventing any new connections from being opened.
 */
void
valent_manager_stop (ValentManager *manager)
{
  g_return_if_fail (VALENT_IS_MANAGER (manager));

  /* We're already stopped */
  if (manager->cancellable == NULL)
    return;

  /* Cancel any running operations */
  g_cancellable_cancel (manager->cancellable);
  g_clear_object (&manager->cancellable);

  /* Stop and remove services */
  g_signal_handlers_disconnect_by_data (manager->engine, manager);
  g_hash_table_remove_all (manager->services);
}

/**
 * valent_manager_export:
 * @manager: a #ValentManager
 * @connection: a #GDBusConnection
 *
 * Export @manager and all managed devices on @connection.
 */
void
valent_manager_export (ValentManager   *manager,
                       GDBusConnection *connection)
{
  GHashTableIter iter;
  ValentDevice *device;

  g_return_if_fail (VALENT_IS_MANAGER (manager));
  g_return_if_fail (G_IS_DBUS_CONNECTION (connection));

  if (manager->dbus != NULL)
    return;

  manager->dbus = g_dbus_object_manager_server_new (APPLICATION_PATH);
  g_dbus_object_manager_server_set_connection (manager->dbus, connection);

  g_hash_table_iter_init (&iter, manager->devices);

  while (g_hash_table_iter_next (&iter, NULL, (void **)&device))
    valent_manager_export_device (manager, device);
}

/**
 * valent_manager_unexport:
 * @manager: a #ValentManager
 *
 * Unexport all managed devices, then @manager from DBus.
 */
void
valent_manager_unexport (ValentManager *manager)
{
  GHashTableIter iter;
  ValentDevice *device;

  g_return_if_fail (VALENT_IS_MANAGER (manager));

  if (manager->dbus == NULL)
    return;

  g_hash_table_iter_init (&iter, manager->devices);

  while (g_hash_table_iter_next (&iter, NULL, (void **)&device))
    valent_manager_unexport_device (manager, device);

  g_dbus_object_manager_server_set_connection (manager->dbus, NULL);
  g_clear_object (&manager->dbus);
}

