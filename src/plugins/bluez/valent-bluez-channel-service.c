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
static void
valent_mux_connection_handshake_cb (ValentMuxConnection *muxer,
                                    GAsyncResult        *result,
                                    gpointer             user_data)
{
  g_autoptr (GTask) task = G_TASK (g_steal_pointer (&user_data));
  ValentBluezChannelService *self = g_task_get_source_object (task);
  const char *object_path = g_task_get_task_data (task);
  g_autoptr (ValentChannel) channel = NULL;
  g_autoptr (GError) error = NULL;

  channel = valent_mux_connection_handshake_finish (muxer, result, &error);
  if (channel != NULL)
    {
      g_hash_table_replace (self->muxers,
                            g_strdup (object_path),
                            g_object_ref (muxer));
      valent_channel_service_channel (VALENT_CHANNEL_SERVICE (self), channel);
    }
  else if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
      g_warning ("%s(): \"%s\": %s", G_STRFUNC, object_path, error->message);
    }

  g_task_return_boolean (task, TRUE);
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
  g_autoptr (GTask) task = NULL;

  g_assert (VALENT_IS_BLUEZ_CHANNEL_SERVICE (self));
  g_assert (G_IS_SOCKET_CONNECTION (connection));
  g_assert (object_path != NULL);

  task = g_task_new (self, NULL, NULL, NULL);
  g_task_set_task_data (task, g_strdup (object_path), g_free);
  g_task_set_source_tag (task, on_connection_opened);

  identity = valent_channel_service_ref_identity (VALENT_CHANNEL_SERVICE (self));
  muxer = g_object_new (VALENT_TYPE_MUX_CONNECTION,
                        "base-stream", connection,
                        "buffer-size", DEFAULT_BUFFER_SIZE,
                        NULL);
  valent_mux_connection_handshake (muxer,
                                   identity,
                                   NULL,
                                   (GAsyncReadyCallback)valent_mux_connection_handshake_cb,
                                   g_object_ref (task));
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
          g_hash_table_replace (self->devices,
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

  if (g_hash_table_steal_extended (self->muxers, object_path, &key, &value))
    {
      valent_mux_connection_close (value, NULL, NULL);
      g_free (key);
      g_object_unref (value);
    }

  g_hash_table_remove (self->devices, object_path);
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

  name_owner = g_dbus_proxy_get_name_owner (proxy);
  if (name_owner != NULL)
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
activate_cb (GObject      *object,
             GAsyncResult *result,
             gpointer      user_data)
{
  ValentBluezChannelService *self = VALENT_BLUEZ_CHANNEL_SERVICE (user_data);
  g_autoptr (GError) error = NULL;

  if (g_task_propagate_boolean (G_TASK (result), &error))
    {
      valent_extension_plugin_state_changed (VALENT_EXTENSION (self),
                                             VALENT_PLUGIN_STATE_ACTIVE,
                                             NULL);
    }
  else
    {
      valent_extension_plugin_state_changed (VALENT_EXTENSION (self),
                                             VALENT_PLUGIN_STATE_ERROR,
                                             error);
    }
}

static void
get_managed_objects_cb (GDBusProxy   *proxy,
                        GAsyncResult *result,
                        gpointer      user_data)
{
  g_autoptr (GTask) task = G_TASK (g_steal_pointer (&user_data));
  ValentBluezChannelService *self = g_task_get_source_object (task);
  g_autoptr (GVariant) reply = NULL;
  g_autoptr (GVariant) objects = NULL;
  GError *error = NULL;
  const char *object_path;
  GVariant *interfaces;
  GVariantIter iter;

  reply = g_dbus_proxy_call_finish (proxy, result, &error);
  if (reply == NULL)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  objects = g_variant_get_child_value (reply, 0);
  g_variant_iter_init (&iter, objects);
  while (g_variant_iter_next (&iter, "{&o@a{sa{sv}}}", &object_path, &interfaces))
    {
      on_interfaces_added (self, object_path, interfaces);
      g_variant_unref (interfaces);
    }

  g_task_return_boolean (task, TRUE);
}

static inline void
valent_bluez_profile_register_cb (ValentBluezProfile *profile,
                                  GAsyncResult       *result,
                                  gpointer            user_data)
{
  g_autoptr (GTask) task = G_TASK (g_steal_pointer (&user_data));
  ValentBluezChannelService *self = g_task_get_source_object (task);
  GCancellable *cancellable = g_task_get_cancellable (task);
  GError *error = NULL;

  if (!valent_bluez_profile_register_finish (profile, result, &error))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  g_dbus_proxy_call (self->proxy,
                     "GetManagedObjects",
                     NULL,
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     cancellable,
                     (GAsyncReadyCallback)get_managed_objects_cb,
                     g_object_ref (task));
}

static void
on_name_owner_changed (GDBusProxy                *proxy,
                       GParamSpec                *pspec,
                       ValentBluezChannelService *self)
{
  g_autofree char *name_owner = NULL;

  g_assert (VALENT_IS_BLUEZ_CHANNEL_SERVICE (self));

  name_owner = g_dbus_proxy_get_name_owner (proxy);
  if (name_owner != NULL)
    {
      g_autoptr (GTask) task = NULL;
      GDBusConnection *connection = NULL;

      connection = g_dbus_proxy_get_connection (proxy);
      task = g_task_new (self, NULL, activate_cb, self);
      g_task_set_source_tag (task, on_name_owner_changed);
      g_task_set_task_data (task, g_object_ref (connection), g_object_unref);
      valent_bluez_profile_register (self->profile,
                                     connection,
                                     NULL,
                                     (GAsyncReadyCallback)valent_bluez_profile_register_cb,
                                     g_object_ref (task));
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

      valent_extension_plugin_state_changed (VALENT_EXTENSION (self),
                                             VALENT_PLUGIN_STATE_INACTIVE,
                                             NULL);
    }
}

/*
 * ValentChannelService
 */
static void
valent_bluez_channel_service_build_identity (ValentChannelService *service)
{
  ValentChannelServiceClass *klass;
  g_autoptr (JsonNode) identity = NULL;

  g_assert (VALENT_IS_BLUEZ_CHANNEL_SERVICE (service));

  /* Chain-up */
  klass = VALENT_CHANNEL_SERVICE_CLASS (valent_bluez_channel_service_parent_class);
  klass->build_identity (service);

  /* Set the certificate on the packet
   */
  identity = valent_channel_service_ref_identity (service);
  if (identity != NULL)
    {
      g_autoptr (GTlsCertificate) certificate = NULL;
      g_autofree char *certificate_pem = NULL;
      JsonObject *body;

      certificate = valent_channel_service_ref_certificate (service);
      g_object_get (certificate, "certificate-pem", &certificate_pem, NULL);

      body = valent_packet_get_body (identity);
      json_object_set_string_member (body, "certificate", certificate_pem);
    }
}

static void
valent_bluez_channel_service_identify (ValentChannelService *service,
                                       const char           *target)
{
  ValentBluezChannelService *self = VALENT_BLUEZ_CHANNEL_SERVICE (service);
  g_autofree char *name_owner = NULL;
  ValentBluezDevice *device;

  g_assert (VALENT_IS_BLUEZ_CHANNEL_SERVICE (self));

  name_owner = g_dbus_proxy_get_name_owner (self->proxy);
  if (name_owner == NULL)
    return;

  if (target != NULL)
    {
      device = g_hash_table_lookup (self->devices, target);
      if (device != NULL)
        valent_bluez_device_connect (device);
    }
  else
    {
      GHashTableIter iter;

      g_hash_table_iter_init (&iter, self->devices);
      while (g_hash_table_iter_next (&iter, NULL, (void **)&device))
        valent_bluez_device_connect (device);
    }
}

static void
g_dbus_proxy_new_for_bus_cb (GObject      *object,
                             GAsyncResult *result,
                             gpointer      user_data)
{
  g_autoptr (GTask) task = G_TASK (user_data);
  ValentBluezChannelService *self = g_task_get_source_object (task);
  GError *error = NULL;

  self->proxy = g_dbus_proxy_new_for_bus_finish (result, &error);
  if (self->proxy == NULL)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  g_signal_connect_object (self->proxy,
                           "g-signal",
                           G_CALLBACK (on_g_signal),
                           self,
                           G_CONNECT_DEFAULT);
  g_signal_connect_object (self->proxy,
                           "notify::g-name-owner",
                           G_CALLBACK (on_name_owner_changed),
                           self,
                           G_CONNECT_DEFAULT);
  on_name_owner_changed (self->proxy, NULL, self);

  g_task_return_boolean (task, TRUE);
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

  g_assert (VALENT_IS_CHANNEL_SERVICE (initable));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (initable, cancellable, callback, user_data);
  g_task_set_priority (task, io_priority);
  g_task_set_source_tag (task, valent_bluez_channel_service_init_async);

  g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                            G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START,
                            NULL,
                            "org.bluez",
                            "/",
                            "org.freedesktop.DBus.ObjectManager",
                            cancellable,
                            (GAsyncReadyCallback)g_dbus_proxy_new_for_bus_cb,
                            g_object_ref (task));
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

  service_class->build_identity = valent_bluez_channel_service_build_identity;
  service_class->identify = valent_bluez_channel_service_identify;
}

static void
valent_bluez_channel_service_init (ValentBluezChannelService *self)
{
  self->profile = valent_bluez_profile_new ();
  g_signal_connect_object (self->profile,
                           "connection-opened",
                           G_CALLBACK (on_connection_opened),
                           self,
                           G_CONNECT_DEFAULT);
  g_signal_connect_object (self->profile,
                           "connection-closed",
                           G_CALLBACK (on_connection_closed),
                           self,
                           G_CONNECT_DEFAULT);

  self->devices = g_hash_table_new_full (g_str_hash,
                                         g_str_equal,
                                         g_free,
                                         g_object_unref);
  self->muxers = g_hash_table_new_full (g_str_hash,
                                        g_str_equal,
                                        g_free,
                                        g_object_unref);
}

