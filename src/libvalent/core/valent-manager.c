// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-manager"

#include "config.h"

#include <gio/gio.h>

#include "valent-certificate.h"
#include "valent-channel.h"
#include "valent-channel-service.h"
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
 * SECTION:valent-manager
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
  GHashTable               *services_settings;

  GDBusObjectManagerServer *dbus;
  GHashTable               *exported;
};

static void           valent_manager_add_device    (ValentManager *manager,
                                                    ValentDevice  *device);
static void           valent_manager_remove_device (ValentManager *manager,
                                                    ValentDevice  *device);
static ValentDevice * valent_manager_ensure_device (ValentManager *manager,
                                                    const char    *id,
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
 * Certificate
 */
static gboolean
valent_manager_ensure_certificate (ValentManager  *self,
                                   GCancellable   *cancellable,
                                   GError        **error)
{
  g_autoptr (GFile) cert_file = NULL;
  g_autoptr (GFile) key_file = NULL;
  const char *cert_path;
  const char *key_path;

  /* Check if the certificate has already been generated */
  cert_file = valent_data_new_config_file (self->data, "certificate.pem");
  key_file = valent_data_new_config_file (self->data, "private.pem");

  cert_path = g_file_peek_path (cert_file);
  key_path = g_file_peek_path (key_file);

  /* Generate new certificate with gnutls */
  if (!g_file_query_exists (cert_file, cancellable) ||
      !g_file_query_exists (key_file, cancellable))
    {
      g_autofree char *common_name = NULL;

      common_name = g_uuid_string_random ();

      if (!valent_certificate_generate (key_path, cert_path, common_name, error))
        return FALSE;
    }

  /* Load the service certificate */
  self->certificate = g_tls_certificate_new_from_files (cert_path,
                                                        key_path,
                                                        error);

  if (self->certificate == NULL)
    return FALSE;

  /* Extract our deviceId from the certificate */
  self->id = valent_certificate_get_common_name (self->certificate);

  if (self->id == NULL)
    {
      g_set_error_literal (error,
                           G_TLS_ERROR,
                           G_TLS_ERROR_BAD_CERTIFICATE,
                           "Certificate has no common name");
      return FALSE;
    }

  return G_IS_TLS_CERTIFICATE (self->certificate);
}

/*
 * DBus
 */
typedef struct
{
  char  *path;
  guint  actions_id;
  guint  menu_id;
} ExportedDevice;

static char *
get_device_dbus_path (ValentDevice *device)
{
  guint len = 0;
  char dbus_id[256] = { 0, };
  const char *id;

  g_assert (VALENT_IS_DEVICE (device));

  id = valent_device_get_id (device);

  while (*id)
    {
      if G_LIKELY (g_ascii_isalnum (*id))
        dbus_id[len++] = *id;
      else
        dbus_id[len++] = '_';

      id++;
    }
  dbus_id[len] = '\0';

  return g_strdup_printf (APPLICATION_PATH"/Device/%s", dbus_id);
}

static void
valent_manager_export_device (ValentManager *manager,
                              ValentDevice  *device)
{
  g_autoptr (GDBusConnection) connection = NULL;
  g_autoptr (GDBusObjectSkeleton) object = NULL;
  g_autoptr (GDBusInterfaceSkeleton) iface = NULL;
  ExportedDevice *exported;
  GActionGroup *actions;
  GMenuModel *menu;

  VALENT_ENTRY;

  g_assert (VALENT_IS_MANAGER (manager));
  g_assert (VALENT_IS_DEVICE (device));

  if (g_hash_table_contains (manager->exported, device))
    VALENT_EXIT;

  connection = g_dbus_object_manager_server_get_connection (manager->dbus);

  exported = g_new0 (ExportedDevice, 1);
  exported->path = get_device_dbus_path (device);

  /* Device Interface */
  object = g_dbus_object_skeleton_new (exported->path);
  iface = (GDBusInterfaceSkeleton *)valent_remote_device_skeleton_new ();

  g_object_bind_property (device, "connected",
                          iface,  "connected",
                          G_BINDING_SYNC_CREATE);
  g_object_bind_property (device, "paired",
                          iface,  "paired",
                          G_BINDING_SYNC_CREATE);
  g_object_bind_property (device, "icon-name",
                          iface,  "icon-name",
                          G_BINDING_SYNC_CREATE);
  g_object_bind_property (device, "id",
                          iface,  "id",
                          G_BINDING_SYNC_CREATE);
  g_object_bind_property (device, "name",
                          iface,  "name",
                          G_BINDING_SYNC_CREATE);
  g_object_bind_property (device, "type",
                          iface,  "type",
                          G_BINDING_SYNC_CREATE);

  g_dbus_object_skeleton_add_interface (object, iface);

  /* GActionGroup */
  actions = valent_device_get_actions (device);
  exported->actions_id = g_dbus_connection_export_action_group (connection,
                                                                exported->path,
                                                                actions,
                                                                NULL);

  /* GMenuModel */
  menu = valent_device_get_menu (device);
  exported->menu_id = g_dbus_connection_export_menu_model (connection,
                                                           exported->path,
                                                           menu,
                                                           NULL);

  g_dbus_object_manager_server_export (manager->dbus, object);
  g_hash_table_insert (manager->exported, device, exported);

  VALENT_EXIT;
}

static void
valent_manager_unexport_device (ValentManager *manager,
                                ValentDevice  *device)
{
  g_autoptr (GDBusConnection) connection = NULL;
  gpointer data;
  ExportedDevice *exported = NULL;

  VALENT_ENTRY;

  g_assert (VALENT_IS_MANAGER (manager));
  g_assert (VALENT_IS_DEVICE (device));

  if (!g_hash_table_steal_extended (manager->exported, device, NULL, &data))
    VALENT_EXIT;

  exported = (ExportedDevice *)data;
  connection = g_dbus_object_manager_server_get_connection (manager->dbus);

  g_dbus_object_manager_server_unexport (manager->dbus, exported->path);
  g_dbus_connection_unexport_action_group (connection, exported->actions_id);
  g_dbus_connection_unexport_menu_model (connection, exported->menu_id);

  g_clear_pointer (&exported->path, g_free);
  g_free (exported);

  VALENT_EXIT;
}

/*
 * Channel Services
 */
static void
on_channel (ValentChannelService *service,
            ValentChannel        *channel,
            ValentManager        *manager)
{
  JsonNode *identity;
  const char *device_id;
  g_autoptr (ValentDevice) device = NULL;

  VALENT_ENTRY;

  g_assert (VALENT_IS_CHANNEL_SERVICE (service));
  g_assert (VALENT_IS_CHANNEL (channel));
  g_assert (VALENT_IS_MANAGER (manager));

  identity = valent_channel_get_peer_identity (channel);

  if G_UNLIKELY (identity == NULL)
    {
      g_warning ("%s: missing peer identity", G_OBJECT_TYPE_NAME (channel));
      return;
    }

  device_id = valent_identity_get_device_id (identity);

  if G_UNLIKELY (device_id == NULL)
    {
      g_warning ("%s: missing deviceId", G_OBJECT_TYPE_NAME (channel));
      return;
    }

  device = valent_manager_ensure_device (manager, device_id, NULL);
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
  g_assert (G_IS_ASYNC_RESULT (result));

  if (!valent_channel_service_start_finish (service, result, &error) &&
      !valent_error_ignore (error))
    g_warning ("%s: %s", G_OBJECT_TYPE_NAME (service), error->message);

  VALENT_EXIT;
}

static void
valent_manager_enable_service (ValentManager  *manager,
                               PeasPluginInfo *info)
{
  PeasExtension *service;
  const char *module;

  g_assert (VALENT_IS_MANAGER (manager));

  service = peas_engine_create_extension (manager->engine,
                                          info,
                                          VALENT_TYPE_CHANNEL_SERVICE,
                                          "data", manager->data,
                                          "id",   manager->id,
                                          NULL);

  if (service == NULL)
    return;

  /* Track the service */
  module = peas_plugin_info_get_module_name (info);
  g_hash_table_insert (manager->services, (char *)module, service);

  /* Start the service */
  g_signal_connect_object (service,
                           "channel",
                           G_CALLBACK (on_channel),
                           manager, 0);

  valent_channel_service_start (VALENT_CHANNEL_SERVICE (service),
                                manager->cancellable,
                                (GAsyncReadyCallback)valent_channel_service_start_cb,
                                manager);
}

static void
valent_manager_disable_service (ValentManager  *manager,
                                PeasPluginInfo *info)
{
  gpointer service;
  const char *module;

  module = peas_plugin_info_get_module_name (info);

  if (g_hash_table_steal_extended (manager->services, module, NULL, &service))
    {
      valent_channel_service_stop (service);
      g_signal_handlers_disconnect_by_data (service, manager);
      g_object_unref (service);
    }
}

typedef struct
{
  ValentManager *manager;
  PeasPluginInfo      *info;
} ServiceInfo;

static void
on_enabled_changed (GSettings   *settings,
                    const char  *key,
                    ServiceInfo *service_info)
{
  ValentManager *manager = service_info->manager;
  PeasPluginInfo *info = service_info->info;

  g_assert (G_IS_SETTINGS (settings));
  g_assert (VALENT_IS_MANAGER (manager));

  if (g_settings_get_boolean (settings, key))
    valent_manager_enable_service (manager, info);
  else
    valent_manager_disable_service (manager, info);
}

static void
on_load_service (PeasEngine     *engine,
                 PeasPluginInfo *info,
                 ValentManager  *manager)
{
  ServiceInfo *service_info;
  const char *module;
  g_autofree char *path = NULL;
  GSettings *settings;

  g_assert (PEAS_IS_ENGINE (engine));
  g_assert (info != NULL);
  g_assert (VALENT_IS_MANAGER (manager));

  /* We're only interested in one GType */
  if (!peas_engine_provides_extension (engine, info, VALENT_TYPE_CHANNEL_SERVICE))
    return;

  /* We create and destroy the PeasExtension based on the enabled state */
  module = peas_plugin_info_get_module_name (info);
  path = g_strdup_printf ("/ca/andyholmes/valent/network/%s/", module);
  settings = g_settings_new_with_path ("ca.andyholmes.Valent.Plugin", path);
  g_hash_table_insert (manager->services_settings, info, settings);

  /* Watch for enabled/disabled */
  service_info = g_new0 (ServiceInfo, 1);
  service_info->manager = manager;
  service_info->info = info;

  g_signal_connect_data (settings,
                         "changed::enabled",
                         G_CALLBACK (on_enabled_changed),
                         service_info,
                         (GClosureNotify)g_free,
                         0);

  if (g_settings_get_boolean (settings, "enabled"))
    valent_manager_enable_service (manager, info);
}

static void
on_unload_service (PeasEngine     *engine,
                   PeasPluginInfo *info,
                   ValentManager  *manager)
{
  g_assert (PEAS_IS_ENGINE (engine));
  g_assert (info != NULL);
  g_assert (VALENT_IS_MANAGER (manager));

  /* We're only interested in one GType */
  if (!peas_engine_provides_extension (engine, info, VALENT_TYPE_CHANNEL_SERVICE))
    return;

  if (g_hash_table_remove (manager->services_settings, info))
    valent_manager_disable_service (manager, info);
}


/*
 * Cached Devices
 */
typedef struct
{
  ValentManager *manager;
  JsonNode      *identity;
} CachedDevice;

static void
cached_device_free (gpointer data)
{
  CachedDevice *cache = data;

  g_clear_object (&cache->manager);
  g_clear_pointer (&cache->identity, json_node_unref);
  g_free (cache);
}

static gboolean
ensure_device_main (gpointer user_data)
{
  CachedDevice *data = user_data;
  g_autoptr (ValentDevice) device = NULL;
  const char *device_id;

  g_assert (VALENT_IS_MAIN_THREAD ());

  device_id = valent_identity_get_device_id (data->identity);
  device = valent_manager_ensure_device (data->manager,
                                         device_id,
                                         data->identity);

  return G_SOURCE_REMOVE;
}

static gboolean
valent_manager_load_devices (ValentManager  *self,
                             GCancellable   *cancellable,
                             GError        **error)
{
  g_autoptr (GFile) config_dir = NULL;
  g_autoptr (GFileEnumerator) iter = NULL;
  g_autoptr (JsonParser) parser = NULL;

  /* Look in the config directory for subdirectories. We only fail on
   * G_IO_ERROR_CANCELLED; all other errors mean there are just no devices. */
  config_dir = g_file_new_for_path (valent_data_get_config_path (self->data));
  iter = g_file_enumerate_children (config_dir,
                                    G_FILE_ATTRIBUTE_STANDARD_NAME,
                                    G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                    cancellable,
                                    error);

  if (iter == NULL)
    {
      if (g_error_matches (*error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        return FALSE;

      g_clear_error (error);
      return TRUE;
    }

  /* Iterate the subdirectories looking for identity.json files */
  parser = json_parser_new ();

  while (TRUE)
    {
      GFile *dir;
      g_autoptr (GFile) identity = NULL;

      if (!g_file_enumerator_iterate (iter, NULL, &dir, cancellable, error))
        {
          if (g_error_matches (*error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
            return FALSE;

          g_clear_error (error);
          return TRUE;
        }

      if (dir == NULL)
        break;

      /* Check if identity.json exists */
      identity = g_file_get_child (dir, "identity.json");

      if (g_file_query_exists (identity, cancellable))
        {
          g_autoptr (GError) warning = NULL;
          const char *path;
          CachedDevice *cached_device;

          /* Try parsing the identity */
          path = g_file_peek_path (identity);

          if (!json_parser_load_from_file (parser, path, &warning))
            {
              g_warning ("Loading device '%s': %s", path, warning->message);
              continue;
            }

          /* Do the rest in the main context */
          cached_device = g_new0 (CachedDevice, 1);
          cached_device->manager = g_object_ref (self);
          cached_device->identity = json_parser_steal_root (parser);

          g_main_context_invoke_full (NULL,
                                      G_PRIORITY_DEFAULT,
                                      ensure_device_main,
                                      cached_device,
                                      cached_device_free);
        }
    }

  return TRUE;
}

/*
 * Device Management
 */
static void
on_device_state (ValentDevice  *device,
                 GParamSpec    *pspec,
                 ValentManager *manager)
{
  if (valent_device_get_connected (device) || valent_device_get_paired (device))
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

  /* Add the device */
  device_id = valent_device_get_id (device);

  if (g_hash_table_contains (manager->devices, device_id))
    VALENT_EXIT;

  /* Watch for disconnects and unpairs */
  g_signal_connect_object (device,
                           "notify::connected",
                           G_CALLBACK (on_device_state),
                           manager,
                           0);
  g_signal_connect_object (device,
                           "notify::paired",
                           G_CALLBACK (on_device_state),
                           manager,
                           0);

  /* Notify locally, then DBus clients */
  g_hash_table_insert (manager->devices, g_strdup (device_id), g_object_ref (device));
  g_signal_emit (G_OBJECT (manager), signals [DEVICE_ADDED], 0, device);

  if (manager->dbus != NULL)
    valent_manager_export_device (manager, device);

  VALENT_EXIT;
}

static void
valent_manager_remove_device (ValentManager *manager,
                              ValentDevice  *device)
{
  const char *device_id;

  VALENT_ENTRY;

  g_assert (VALENT_IS_MANAGER (manager));
  g_assert (VALENT_IS_DEVICE (device));

  g_object_ref (device);

  /* Remove the device */
  device_id = valent_device_get_id (device);
  g_hash_table_remove (manager->devices, device_id);

  /* Notify locally, then DBus clients */
  g_signal_emit (G_OBJECT (manager), signals [DEVICE_REMOVED], 0, device);
  valent_manager_unexport_device (manager, device);

  g_object_unref (device);

  VALENT_EXIT;
}

static ValentDevice *
valent_manager_ensure_device (ValentManager *manager,
                              const char    *id,
                              JsonNode      *identity)
{
  ValentDevice *device;

  g_assert (VALENT_IS_MANAGER (manager));
  g_assert (id != NULL);
  g_assert (identity == NULL || VALENT_IS_PACKET (identity));

  device = g_hash_table_lookup (manager->devices, id);

  if (device != NULL)
    {
      g_object_ref (device);

      if (identity != NULL)
        valent_device_handle_packet (device, identity);
    }
  else
    {
      g_autoptr (ValentData) data = NULL;

      /* Create a data object with the same base path as the manager */
      data = valent_data_new (id, manager->data);
      device = g_object_new (VALENT_TYPE_DEVICE,
                             "id",   id,
                             "data", data,
                             NULL);

      if (identity != NULL)
        valent_device_handle_packet (device, identity);

      /* Manage the new device */
      valent_manager_add_device (manager, device);
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

  if (self->data == NULL)
    self->data = valent_data_new (NULL, NULL);

  if (!valent_manager_ensure_certificate (self, cancellable, error))
    return FALSE;

  if (!valent_manager_load_devices (self, cancellable, error))
    return FALSE;

  return TRUE;
}


static void
initable_iface_init (GInitableIface *iface)
{
  iface->init = valent_manager_initable_init;
}

static void
async_initable_iface_init (GAsyncInitableIface *iface)
{
}

/*
 * GObject
 */
static void
valent_manager_constructed (GObject *object)
{
  ValentManager *self = VALENT_MANAGER (object);

  if (self->data == NULL)
    self->data = valent_data_new (NULL, NULL);

  G_OBJECT_CLASS (valent_manager_parent_class)->constructed (object);
}

static void
valent_manager_dispose (GObject *object)
{
  ValentManager *self = VALENT_MANAGER (object);
  GHashTableIter iter;
  gpointer value;

  valent_manager_stop (self);
  valent_manager_unexport (self);

  g_hash_table_iter_init (&iter, self->devices);

  while (g_hash_table_iter_next (&iter, NULL, &value))
    {
      g_signal_emit (G_OBJECT (self), signals [DEVICE_REMOVED], 0, value);
      valent_manager_unexport_device (self, VALENT_DEVICE (value));
      g_hash_table_iter_remove (&iter);
    }
}

static void
valent_manager_finalize (GObject *object)
{
  ValentManager *self = VALENT_MANAGER (object);

  g_clear_pointer (&self->exported, g_hash_table_unref);
  g_clear_pointer (&self->services_settings, g_hash_table_unref);
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
      g_value_set_string (value, valent_manager_get_id (self));
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

  object_class->constructed = valent_manager_constructed;
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
   * The unique ID this device will identify as. If not given a random ID will
   * be generated.
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

  self->devices = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
  self->services = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_object_unref);
  self->services_settings = g_hash_table_new_full (NULL, NULL, NULL, g_object_unref);
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
 * @user_data: user supplied data
 *
 * Create a new #ValentManager.
 *
 * This is a failable asynchronous constructor - when the proxy is ready,
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
 * Returns: (transfer container) (element-type Valent.Device): a #GPtrArray
 */
GPtrArray *
valent_manager_get_devices (ValentManager *manager)
{
  GPtrArray *devices;
  GHashTableIter iter;
  gpointer device;

  g_return_val_if_fail (VALENT_IS_MANAGER (manager), NULL);

  devices = g_ptr_array_new ();

  g_hash_table_iter_init (&iter, manager->devices);

  while (g_hash_table_iter_next (&iter, NULL, &device))
    g_ptr_array_add (devices, device);

  return devices;
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
 * The @uri argument is string in the form `backend://address`, where "backend"
 * is the module name and "address" is some address, DBus path or serialized
 * data the backend can use to contact the device. A complete URI for the Lan
 * backend might look like `lan://192.168.0.10:1716`.
 */
void
valent_manager_identify (ValentManager *manager,
                         const char    *uri)
{
  g_return_if_fail (VALENT_IS_MANAGER (manager));

  if (uri != NULL)
    {
      ValentChannelService *service = NULL;
      g_auto (GStrv) address = NULL;

      address = g_strsplit (uri, "://", -1);

      if (address[0] != NULL)
        service = g_hash_table_lookup (manager->services, address[0]);

      if (service != NULL && address[1] != NULL)
        valent_channel_service_identify (service, address[1]);
    }
  else
    {
      GHashTableIter iter;
      gpointer service;

      g_hash_table_iter_init (&iter, manager->services);

      while (g_hash_table_iter_next (&iter, NULL, &service))
        valent_channel_service_identify (service, NULL);
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
 * Unload all the #ValentChannelService implementations loaded from the #ValentEngine,
 * thereby preventing any new connections from being opened.
 */
void
valent_manager_stop (ValentManager *manager)
{
  GHashTableIter iter;
  gpointer value;

  /* We're already stopped */
  if (manager->cancellable == NULL)
    return;

  /* Cancel any running operations */
  g_cancellable_cancel (manager->cancellable);
  g_clear_object (&manager->cancellable);

  /* Stop and remove services */
  g_signal_handlers_disconnect_by_data (manager->engine, manager);

  g_hash_table_iter_init (&iter, manager->services);

  while (g_hash_table_iter_next (&iter, NULL, &value))
    {
      valent_channel_service_stop (VALENT_CHANNEL_SERVICE (value));
      g_hash_table_iter_remove (&iter);
    }
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
  gpointer value;

  g_return_if_fail (VALENT_IS_MANAGER (manager));
  g_return_if_fail (G_IS_DBUS_CONNECTION (connection));

  if (manager->dbus != NULL)
    return;

  manager->dbus = g_dbus_object_manager_server_new (APPLICATION_PATH);
  g_dbus_object_manager_server_set_connection (manager->dbus, connection);

  /* Export each known device */
  g_hash_table_iter_init (&iter, manager->devices);

  while (g_hash_table_iter_next (&iter, NULL, &value))
    valent_manager_export_device (manager, VALENT_DEVICE (value));
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
  gpointer value;

  g_return_if_fail (VALENT_IS_MANAGER (manager));

  if (manager->dbus == NULL)
    return;

  /* Unexport each known device */
  g_hash_table_iter_init (&iter, manager->devices);

  while (g_hash_table_iter_next (&iter, NULL, &value))
    valent_manager_unexport_device (manager, VALENT_DEVICE (value));

  /* Release the DBus connection */
  g_dbus_object_manager_server_set_connection (manager->dbus, NULL);
  g_clear_object (&manager->dbus);
}

