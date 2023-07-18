// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-bluez-channel-service"

#include "config.h"

#include <valent.h>

#include "valent-bluez-channel-service.h"
#include "valent-bluez-device.h"
#include "valent-bluez-profile.h"
#include "valent-mux-connection.h"

#define DEFAULT_BUFFER_SIZE 4096


struct _ValentBluezChannelService
{
  ValentChannelService  parent_instance;

  GDBusProxy           *proxy;
  ValentBluezProfile   *profile;
  GHashTable           *devices;
  GHashTable           *muxers;
};

static void   g_async_initable_iface_init (GAsyncInitableIface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (ValentBluezChannelService, valent_bluez_channel_service, VALENT_TYPE_CHANNEL_SERVICE,
                               G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE, g_async_initable_iface_init))


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

  g_assert (VALENT_IS_BLUEZ_CHANNEL_SERVICE (self));
  g_assert (g_variant_is_object_path (task->object_path));

  channel = valent_mux_connection_handshake_finish (muxer, result, &error);

  if (channel != NULL)
    {
      g_hash_table_replace (self->muxers,
                            g_strdup (task->object_path),
                            g_object_ref (muxer));

      valent_channel_service_channel (VALENT_CHANNEL_SERVICE (self), channel);
    }
  else
    {
      g_warning ("%s(): failed to connect to \"%s\": %s",
                 G_STRFUNC,
                 task->object_path,
                 error->message);
    }

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
                        "buffer-size", DEFAULT_BUFFER_SIZE,
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
  GDBusConnection *connection;
  GVariantIter iter;
  const char *interface;
  GVariant *properties;

  connection = g_dbus_proxy_get_connection (self->proxy);
  g_variant_iter_init (&iter, interfaces);

  while (g_variant_iter_next (&iter, "{&s@a{sv}}", &interface, &properties))
    {
      if (g_strcmp0 (interface, "org.bluez.Device1") == 0)
        {
          g_autoptr (ValentBluezDevice) device = NULL;

          device = valent_bluez_device_new (connection,
                                            object_path,
                                            properties);

          g_hash_table_insert (self->devices,
                               g_strdup (object_path),
                               g_object_ref (device));
        }

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
on_g_signal (GDBusProxy                *proxy,
             char                      *sender_name,
             char                      *signal_name,
             GVariant                  *parameters,
             ValentBluezChannelService *self)
{
  g_autofree char *name_owner = NULL;

  g_assert (VALENT_IS_BLUEZ_CHANNEL_SERVICE (self));

  /* Ensure the name is properly owned */
  if ((name_owner = g_dbus_proxy_get_name_owner (proxy)) == NULL)
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
                        GAsyncResult *result,
                        gpointer      user_data)
{
  g_autoptr (GTask) task = G_TASK (user_data);
  ValentBluezChannelService *self = g_task_get_source_object (task);
  g_autoptr (GVariant) reply = NULL;
  g_autoptr (GVariant) objects = NULL;
  GError *error = NULL;
  const char *object_path;
  GVariant *interfaces;
  GVariantIter iter;

  if ((reply = g_dbus_proxy_call_finish (proxy, result, &error)) == NULL)
    return g_task_return_error (task, error);

  objects = g_variant_get_child_value (reply, 0);

  g_variant_iter_init (&iter, objects);

  while (g_variant_iter_next (&iter, "{&o@a{sa{sv}}}", &object_path, &interfaces))
    {
      on_interfaces_added (self, object_path, interfaces);
      g_variant_unref (interfaces);
    }

  g_task_return_boolean (task, TRUE);
}

static void
register_profile_cb (ValentBluezProfile *profile,
                     GAsyncResult       *result,
                     gpointer            user_data)
{
  g_autoptr (GTask) task = G_TASK (user_data);
  ValentBluezChannelService *self = g_task_get_source_object (task);
  GCancellable *cancellable = g_task_get_cancellable (task);
  GError *error = NULL;

  if (!valent_bluez_profile_register_finish (profile, result, &error))
    return g_task_return_error (task, error);

  g_dbus_proxy_call (self->proxy,
                     "GetManagedObjects",
                     NULL,
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     cancellable,
                     (GAsyncReadyCallback)get_managed_objects_cb,
                     g_steal_pointer (&task));
}

static void
on_name_owner_changed (GDBusProxy                *proxy,
                       GParamSpec                *pspec,
                       ValentBluezChannelService *self)
{
  g_autofree char *name_owner = NULL;

  g_assert (VALENT_IS_BLUEZ_CHANNEL_SERVICE (self));

  if ((name_owner = g_dbus_proxy_get_name_owner (proxy)) != NULL)
    {
      g_autoptr (GTask) task = NULL;
      GDBusConnection *connection;

      task = g_task_new (self, NULL, NULL, NULL);

      connection = g_dbus_proxy_get_connection (proxy);
      valent_bluez_profile_register (self->profile,
                                     connection,
                                     NULL,
                                     (GAsyncReadyCallback)register_profile_cb,
                                     g_steal_pointer (&task));
    }
  else
    {
      GHashTableIter iter;
      ValentMuxConnection *connection;

      g_hash_table_iter_init (&iter, self->muxers);

      while (g_hash_table_iter_next (&iter, NULL, (void **)&connection))
        {
          valent_mux_connection_close (connection, NULL, NULL);
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
  g_autofree char *name_owner = NULL;

  g_assert (VALENT_IS_BLUEZ_CHANNEL_SERVICE (self));

  if ((name_owner = g_dbus_proxy_get_name_owner (self->proxy)) == NULL)
    return;

  valent_object_lock (VALENT_OBJECT (self));

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

  valent_object_unlock (VALENT_OBJECT (self));
}

static void
g_dbus_proxy_new_for_bus_cb (GObject      *object,
                             GAsyncResult *result,
                             gpointer      user_data)
{
  g_autoptr (GTask) task = G_TASK (user_data);
  ValentBluezChannelService *self = g_task_get_source_object (task);
  GCancellable *cancellable = g_task_get_cancellable (task);
  g_autofree char *name_owner = NULL;
  GDBusConnection *connection = NULL;
  GError *error = NULL;

  if ((self->proxy = g_dbus_proxy_new_for_bus_finish (result, &error)) == NULL)
    return g_task_return_error (task, error);

  g_signal_connect_object (self->proxy,
                           "g-signal",
                           G_CALLBACK (on_g_signal),
                           self, 0);

  /* Watch for bluez, but return if it's down currently */
  g_signal_connect_object (self->proxy,
                           "notify::g-name-owner",
                           G_CALLBACK (on_name_owner_changed),
                           self, 0);

  if ((name_owner = g_dbus_proxy_get_name_owner (self->proxy)) == NULL)
    return g_task_return_boolean (task, TRUE);

  /* Make sure we're ready when the profile begins accepting connections */
  g_signal_connect_object (self->profile,
                           "connection-opened",
                           G_CALLBACK (on_connection_opened),
                           self, 0);
  g_signal_connect_object (self->profile,
                           "connection-closed",
                           G_CALLBACK (on_connection_closed),
                           self, 0);

  connection = g_dbus_proxy_get_connection (self->proxy);
  valent_bluez_profile_register (self->profile,
                                 connection,
                                 cancellable,
                                 (GAsyncReadyCallback)register_profile_cb,
                                 g_steal_pointer (&task));
}

/*
 * GAsyncInitable
 */
static void
valent_bluez_channel_service_init_async (GAsyncInitable      *initable,
                                         int                  io_priority,
                                         GCancellable        *cancellable,
                                         GAsyncReadyCallback  callback,
                                         gpointer             user_data)
{
  g_autoptr (GTask) task = NULL;
  g_autoptr (GCancellable) destroy = NULL;

  g_assert (VALENT_IS_CHANNEL_SERVICE (initable));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  /* Cancel initialization if the object is destroyed */
  destroy = valent_object_chain_cancellable (VALENT_OBJECT (initable),
                                             cancellable);

  task = g_task_new (initable, destroy, callback, user_data);
  g_task_set_priority (task, io_priority);
  g_task_set_source_tag (task, valent_bluez_channel_service_init_async);

  g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                            G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START,
                            NULL,
                            "org.bluez",
                            "/",
                            "org.freedesktop.DBus.ObjectManager",
                            destroy,
                            (GAsyncReadyCallback)g_dbus_proxy_new_for_bus_cb,
                            g_steal_pointer (&task));
}

static void
g_async_initable_iface_init (GAsyncInitableIface *iface)
{
  iface->init_async = valent_bluez_channel_service_init_async;
}

/*
 * ValentObject
 */
static void
valent_bluez_channel_service_destroy (ValentObject *object)
{
  ValentBluezChannelService *self = VALENT_BLUEZ_CHANNEL_SERVICE (object);

  if (self->proxy != NULL)
    {
      g_signal_handlers_disconnect_by_data (self->proxy, self);
      g_clear_object (&self->proxy);
    }

  if (self->profile != NULL)
    {
      g_signal_handlers_disconnect_by_data (self->profile, self);
      valent_bluez_profile_unregister (self->profile);
    }

  VALENT_OBJECT_CLASS (valent_bluez_channel_service_parent_class)->destroy (object);
}

/*
 * GObject
 */
static void
valent_bluez_channel_service_finalize (GObject *object)
{
  ValentBluezChannelService *self = VALENT_BLUEZ_CHANNEL_SERVICE (object);

  g_clear_object (&self->proxy);
  g_clear_object (&self->profile);
  g_clear_pointer (&self->devices, g_hash_table_unref);
  g_clear_pointer (&self->muxers, g_hash_table_unref);

  G_OBJECT_CLASS (valent_bluez_channel_service_parent_class)->finalize (object);
}

static void
valent_bluez_channel_service_class_init (ValentBluezChannelServiceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentObjectClass *vobject_class = VALENT_OBJECT_CLASS (klass);
  ValentChannelServiceClass *service_class = VALENT_CHANNEL_SERVICE_CLASS (klass);

  object_class->finalize = valent_bluez_channel_service_finalize;

  vobject_class->destroy = valent_bluez_channel_service_destroy;

  service_class->identify = valent_bluez_channel_service_identify;
}

static void
valent_bluez_channel_service_init (ValentBluezChannelService *self)
{
  self->profile = g_object_new (VALENT_TYPE_BLUEZ_PROFILE, NULL);

  /* Bluez Devices */
  self->devices = g_hash_table_new_full (g_str_hash,
                                         g_str_equal,
                                         g_free,
                                         g_object_unref);

  /* Muxer Connections */
  self->muxers = g_hash_table_new_full (g_str_hash,
                                        g_str_equal,
                                        g_free,
                                        g_object_unref);
}

