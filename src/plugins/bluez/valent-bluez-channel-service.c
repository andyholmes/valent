// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-bluez-channel-service"

#include "config.h"

#include <libpeas/peas.h>
#include <libvalent-core.h>

#include "valent-bluez-channel-service.h"
#include "valent-bluez-device.h"
#include "valent-bluez-profile.h"
#include "valent-mux-connection.h"


struct _ValentBluezChannelService
{
  ValentChannelService  parent_instance;

  GSettings            *settings;

  GRecMutex             mutex;
  GDBusConnection      *connection;
  GDBusProxy           *proxy;
  GHashTable           *devices;

  ValentBluezProfile   *profile;
  GHashTable           *muxers;
};

G_DEFINE_TYPE (ValentBluezChannelService, valent_bluez_channel_service, VALENT_TYPE_CHANNEL_SERVICE)


/*
 * ValentMuxConnection Callbacks
 */
typedef struct
{
  ValentBluezChannelService *service;
  char                      *object_path;
} HandshakeData;

static void
handshake_cb (ValentMuxConnection *muxer,
              GAsyncResult        *result,
              HandshakeData       *task)
{
  ValentBluezChannelService *self = VALENT_BLUEZ_CHANNEL_SERVICE (task->service);
  g_autoptr (ValentChannel) channel = NULL;
  g_autoptr (GError) error = NULL;

  g_assert (VALENT_IS_BLUEZ_CHANNEL_SERVICE (task->service));
  g_assert (g_variant_is_object_path (task->object_path));

  /* On success emit ValentChannelService::channel */
  if ((channel = valent_mux_connection_handshake_finish (muxer, result, &error)))
    {
      g_hash_table_replace (self->muxers,
                            g_strdup (task->object_path),
                            g_object_ref (muxer));

      valent_channel_service_emit_channel (VALENT_CHANNEL_SERVICE (self), channel);
    }
  else
    g_debug ("[%s] %s: %s", G_STRFUNC, task->object_path, error->message);

  /* The channel owns the muxer now */
  g_clear_object (&task->service);
  g_clear_pointer (&task->object_path, g_free);
  g_free (task);
}

/*
 * ValentBluezProfile Callbacks
 */
static void
on_connection_opened (ValentBluezProfile *profile,
                      GSocketConnection  *connection,
                      const char         *object_path,
                      gpointer            user_data)
{
  ValentBluezChannelService *self = VALENT_BLUEZ_CHANNEL_SERVICE (user_data);
  g_autoptr (ValentMuxConnection) muxer = NULL;
  g_autoptr (JsonNode) identity = NULL;
  HandshakeData *task;

  g_assert (VALENT_IS_BLUEZ_CHANNEL_SERVICE (self));
  g_assert (G_IS_SOCKET_CONNECTION (connection));
  g_assert (object_path != NULL);

  /* Create a Muxer */
  muxer = g_object_new (VALENT_TYPE_MUX_CONNECTION,
                        "base-stream", connection,
                        "buffer-size", 4096,
                        NULL);

  /* Negotiate the connection */
  task = g_new0 (HandshakeData, 1);
  task->service = g_object_ref (self);
  task->object_path = g_strdup (object_path);

  identity = valent_channel_service_ref_identity (VALENT_CHANNEL_SERVICE (self));
  valent_mux_connection_handshake_async (muxer,
                                         identity,
                                         NULL,
                                         (GAsyncReadyCallback)handshake_cb,
                                         task);
}

static void
on_connection_closed (ValentBluezProfile *profile,
                      const char         *object_path,
                      gpointer            user_data)
{
  ValentBluezChannelService *self = VALENT_BLUEZ_CHANNEL_SERVICE (user_data);
  gpointer key, value;

  g_assert (VALENT_IS_BLUEZ_CHANNEL_SERVICE (self));
  g_assert (object_path != NULL);

  if (g_hash_table_steal_extended (self->muxers, object_path, &key, &value))
    {
      valent_mux_connection_close (value, NULL, NULL);
      g_free (key);
      g_object_unref (value);
    }
}


/*
 * GDBusObjectManager
 */
static void
on_interfaces_added (ValentBluezChannelService *self,
                     const char                *object_path,
                     GVariant                  *interfaces)
{
  const char *interface;
  GVariant *properties;
  GVariantIter iter;

  g_variant_iter_init (&iter, interfaces);

  while (g_variant_iter_next (&iter, "{&s@a{sv}}", &interface, &properties))
    {
      if (g_strcmp0 (interface, "org.bluez.Device1") == 0)
        {
          g_autoptr (GError) error = NULL;
          g_autoptr (ValentBluezDevice) device = NULL;

          device = valent_bluez_device_new (self->connection,
                                            object_path,
                                            properties,
                                            &error);

          if (device != NULL)
            g_hash_table_insert (self->devices,
                                 g_strdup (object_path),
                                 g_object_ref (device));
          else
            g_warning ("Failed to create Bluez device: %s", error->message);
        }

      else if (g_strcmp0 (interface, "org.bluez.ProfileManager1") == 0)
        VALENT_TODO ("ProfileManager1 interface ready; register profile here?");

      g_variant_unref (properties);
    }
}

static void
on_interfaces_removed (ValentBluezChannelService  *self,
                       const char                 *object_path,
                       const char                **interfaces)
{
  gpointer key, value;

  g_assert (VALENT_IS_BLUEZ_CHANNEL_SERVICE (self));
  g_assert (object_path != NULL);

  if (!g_strv_contains (interfaces, "org.bluez.Device1"))
    return;

  g_hash_table_remove (self->devices, object_path);

  if (g_hash_table_steal_extended (self->muxers, object_path, &key, &value))
    {
      valent_mux_connection_close (value, NULL, NULL);

      g_free (key);
      g_object_unref (value);
    }
}

static void
on_g_signal (GDBusProxy *proxy,
             char       *sender_name,
             char       *signal_name,
             GVariant   *parameters,
             gpointer    user_data)
{
  ValentBluezChannelService *self = VALENT_BLUEZ_CHANNEL_SERVICE (user_data);

  g_assert (VALENT_IS_BLUEZ_CHANNEL_SERVICE (self));

  /* Ensure the name is properly owned */
  if (g_dbus_proxy_get_name_owner (proxy) == NULL)
    return;

  if (g_strcmp0 (signal_name, "InterfacesAdded") == 0)
    {
      const char *object_path;
      GVariant *ifaces_props;

      g_variant_get (parameters, "(&o@a{sa{sv}})", &object_path, &ifaces_props);

      on_interfaces_added (self, object_path, ifaces_props);
    }
  else if (g_strcmp0 (signal_name, "InterfacesRemoved") == 0)
    {
      const char *object_path;
      const char **interfaces;

      g_variant_get (parameters, "(&o^a&s)", &object_path, &interfaces);

      on_interfaces_removed (self, object_path, interfaces);
    }
}

static void
get_managed_objects_cb (GDBusProxy   *proxy,
                        GAsyncResult *res,
                        gpointer      user_data)
{
  ValentBluezChannelService *self = VALENT_BLUEZ_CHANNEL_SERVICE (user_data);
  g_autoptr (GVariant) value = NULL;
  g_autoptr (GVariant) arg0 = NULL;
  g_autoptr (GError) error = NULL;
  const char *object_path;
  GVariant *interfaces;
  GVariantIter iter;

  g_assert (VALENT_IS_BLUEZ_CHANNEL_SERVICE (self));

  value = g_dbus_proxy_call_finish (proxy, res, &error);

  if G_UNLIKELY (value == NULL)
    {
      g_warning ("[%s] %s", G_STRFUNC, error->message);
      return;
    }

  arg0 = g_variant_get_child_value (value, 0);

  g_variant_iter_init (&iter, arg0);

  while (g_variant_iter_next (&iter, "{&o@a{sa{sv}}}", &object_path, &interfaces))
    {
      on_interfaces_added (self, object_path, interfaces);
      g_variant_unref (interfaces);
    }
}

static void
on_name_owner_changed (GDBusProxy *proxy,
                       GParamSpec *pspec,
                       gpointer    user_data)
{
  ValentBluezChannelService *self = VALENT_BLUEZ_CHANNEL_SERVICE (user_data);
  g_autofree char *name_owner = NULL;

  g_assert (VALENT_IS_BLUEZ_CHANNEL_SERVICE (self));

  if ((name_owner = g_dbus_proxy_get_name_owner (proxy)) != NULL)
    {
      g_dbus_proxy_call (self->proxy,
                         "GetManagedObjects",
                         NULL,
                         G_DBUS_CALL_FLAGS_NONE,
                         -1,
                         NULL,
                         (GAsyncReadyCallback)get_managed_objects_cb,
                         self);
    }
  else
    {
      GHashTableIter iter;
      gpointer value;

      g_hash_table_iter_init (&iter, self->muxers);

      while (g_hash_table_iter_next (&iter, NULL, &value))
        {
          valent_mux_connection_close (value, NULL, NULL);
          g_hash_table_iter_remove (&iter);
        }

      g_hash_table_remove_all (self->devices);
    }
}


/*
 * ValentChannelService
 */
static void
valent_bluez_channel_service_identify (ValentChannelService *service,
                                       const char           *target)
{
  ValentBluezChannelService *self = VALENT_BLUEZ_CHANNEL_SERVICE (service);

  g_assert (VALENT_IS_BLUEZ_CHANNEL_SERVICE (self));

  if (g_dbus_proxy_get_name_owner (self->proxy) == NULL)
    return;

  g_rec_mutex_lock (&self->mutex);

  if (target != NULL)
    {
      ValentBluezDevice *device;

      device = g_hash_table_lookup (self->devices, target);

      if (device != NULL && valent_bluez_device_is_supported (device))
        valent_bluez_device_connect (device);
    }
  else
    {
      GHashTableIter iter;
      gpointer device;

      g_hash_table_iter_init (&iter, self->devices);

      while (g_hash_table_iter_next (&iter, NULL, &device))
        {
          if (valent_bluez_device_is_supported (device))
            valent_bluez_device_connect (device);
        }
    }

  g_rec_mutex_unlock (&self->mutex);
}

static void
start_task (GTask        *task,
            gpointer      source_object,
            gpointer      task_data,
            GCancellable *cancellable)
{
  ValentBluezChannelService *self = source_object;
  GError *error = NULL;

  /* Get the system bus connection */
  if (self->connection == NULL)
    self->connection = g_bus_get_sync (G_BUS_TYPE_SYSTEM, cancellable, &error);

  if (self->connection == NULL)
    return g_task_return_error (task, error);

  /* Bluez ObjectManager */
  self->proxy = g_dbus_proxy_new_sync (self->connection,
                                       G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START,
                                       NULL,
                                       "org.bluez",
                                       "/",
                                       "org.freedesktop.DBus.ObjectManager",
                                       cancellable,
                                       &error);

  if (self->proxy == NULL)
    return g_task_return_error (task, error);

  g_object_connect (self->proxy,
                    "signal::notify::g-name-owner", on_name_owner_changed, self,
                    "signal::g-signal",             on_g_signal,           self,
                    NULL);
  on_name_owner_changed (self->proxy, NULL, self);

  /* Bluetooth service profile */
  g_object_connect (self->profile,
                    "signal::connection-opened", on_connection_opened, self,
                    "signal::connection-closed", on_connection_closed, self,
                    NULL);

  if (!valent_bluez_profile_register (self->profile,
                                      self->connection,
                                      cancellable,
                                      &error))
    {
      g_signal_handlers_disconnect_by_data (self->profile, self);
      g_signal_handlers_disconnect_by_data (self->proxy, self);
      g_clear_object (&self->proxy);

      return g_task_return_error (task, error);
    }

  return g_task_return_boolean (task, TRUE);
}

static void
valent_bluez_channel_service_start (ValentChannelService *service,
                                    GCancellable         *cancellable,
                                    GAsyncReadyCallback   callback,
                                    gpointer              user_data)
{
  g_autoptr (GTask) task = NULL;

  g_assert (VALENT_IS_CHANNEL_SERVICE (service));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (service, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_bluez_channel_service_start);
  g_task_run_in_thread (task, start_task);
}

static void
valent_bluez_channel_service_stop (ValentChannelService *service)
{
  ValentBluezChannelService *self = VALENT_BLUEZ_CHANNEL_SERVICE (service);

  g_assert (VALENT_IS_BLUEZ_CHANNEL_SERVICE (service));

  if (self->proxy != NULL)
    {
      g_signal_handlers_disconnect_by_data (self->proxy, self);
      g_clear_object (&self->proxy);
    }

  g_signal_handlers_disconnect_by_data (self->profile, self);
  valent_bluez_profile_unregister (self->profile);
}


/*
 * GObject
 */
static void
valent_bluez_channel_service_finalize (GObject *object)
{
  ValentBluezChannelService *self = VALENT_BLUEZ_CHANNEL_SERVICE (object);

  g_clear_object (&self->settings);

  /* Bluez */
  g_clear_object (&self->connection);
  g_clear_object (&self->proxy);
  g_clear_object (&self->profile);
  g_clear_pointer (&self->devices, g_hash_table_unref);

  /* Muxers */
  g_clear_pointer (&self->muxers, g_hash_table_unref);
  g_clear_object (&self->settings);
  g_rec_mutex_clear (&self->mutex);

  G_OBJECT_CLASS (valent_bluez_channel_service_parent_class)->finalize (object);
}

static void
valent_bluez_channel_service_class_init (ValentBluezChannelServiceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentChannelServiceClass *service_class = VALENT_CHANNEL_SERVICE_CLASS (klass);

  object_class->finalize = valent_bluez_channel_service_finalize;

  service_class->identify = valent_bluez_channel_service_identify;
  service_class->start = valent_bluez_channel_service_start;
  service_class->stop = valent_bluez_channel_service_stop;
}

static void
valent_bluez_channel_service_init (ValentBluezChannelService *self)
{
  g_rec_mutex_init (&self->mutex);
  self->connection = NULL;
  self->profile = g_object_new (VALENT_TYPE_BLUEZ_PROFILE, NULL);
  self->settings = g_settings_new ("ca.andyholmes.valent.bluez");

  /* Bluez Devices */
  self->devices = g_hash_table_new_full (g_str_hash, g_str_equal,
                                         g_free, g_object_unref);

  /* Muxers */
  self->muxers = g_hash_table_new_full (g_str_hash, g_str_equal,
                                        g_free, g_object_unref);
}

