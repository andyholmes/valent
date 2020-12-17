// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-upower-device-provider"

#include "config.h"

#include <libpeas/peas.h>
#include <libvalent-core.h>
#include <libvalent-power.h>

#include "valent-upower-device.h"
#include "valent-upower-device-provider.h"


struct _ValentUPowerDeviceProvider
{
  ValentPowerDeviceProvider  parent_instance;

  GDBusProxy                *upower;
  GCancellable              *cancellable;
  GHashTable                *devices;
};

G_DEFINE_TYPE (ValentUPowerDeviceProvider, valent_upower_device_provider, VALENT_TYPE_POWER_DEVICE_PROVIDER)


static void
valent_upower_device_add_device (ValentUPowerDeviceProvider *self,
                                 ValentPowerDevice          *device)
{
  g_autofree char *object_path = NULL;

  g_assert (VALENT_IS_POWER_DEVICE_PROVIDER (self));
  g_assert (VALENT_IS_POWER_DEVICE (device));

  g_object_get (device, "object-path", &object_path, NULL);
  g_hash_table_insert (self->devices, g_steal_pointer (&object_path), g_object_ref (device));

  valent_power_device_provider_emit_device_added (VALENT_POWER_DEVICE_PROVIDER (self), device);
}

static void
valent_upower_device_remove_device (ValentUPowerDeviceProvider *self,
                                    const char                 *object_path)
{
  ValentPowerDeviceProvider *provider = VALENT_POWER_DEVICE_PROVIDER (self);
  gpointer key, value;

  g_assert (VALENT_IS_POWER_DEVICE_PROVIDER (self));
  g_assert (g_variant_is_object_path (object_path));

  if (g_hash_table_steal_extended (self->devices, object_path, &key, &value))
    {
      valent_power_device_provider_emit_device_removed (provider, VALENT_POWER_DEVICE (value));
      g_free (key);
      g_object_unref (value);
    }
}

static void
device_cb (GObject                    *object,
           GAsyncResult               *result,
           ValentUPowerDeviceProvider *self)
{
  g_autoptr (ValentPowerDevice) device = NULL;
  g_autoptr (GError) error = NULL;

  g_assert (VALENT_IS_UPOWER_DEVICE_PROVIDER (self));

  if ((device = valent_upower_device_new_finish (result, &error)) != NULL)
    valent_upower_device_add_device (self, device);
  else
    g_warning ("Error adding power device: %s", error->message);
}

static void
on_upower_signal (GDBusProxy                 *proxy,
                  char                       *sender_name,
                  char                       *signal_name,
                  GVariant                   *parameters,
                  ValentUPowerDeviceProvider *self)
{
  const char *object_path = NULL;

  g_assert (VALENT_IS_UPOWER_DEVICE_PROVIDER (self));

  g_variant_get (parameters, "(&o)", object_path);

  if (g_strcmp0 (signal_name, "DeviceAdded") == 0)
    {
      valent_upower_device_new (object_path,
                                self->cancellable,
                                (GAsyncReadyCallback)device_cb,
                                self);
    }
  else if (g_strcmp0 (signal_name, "DeviceRemoved") == 0)
    {
      valent_upower_device_remove_device (self, object_path);
    }
}

static void
enumerate_devices_cb (GDBusProxy                 *proxy,
                      GAsyncResult               *result,
                      ValentUPowerDeviceProvider *self)
{
  g_autoptr (GVariant) reply = NULL;
  g_autoptr (GError) error = NULL;
  g_autofree const char **object_paths = NULL;

  g_assert (VALENT_IS_UPOWER_DEVICE_PROVIDER (self));

  if ((reply = g_dbus_proxy_call_finish (proxy, result, &error)) == NULL)
    {
      g_warning ("[%s] %s", G_STRFUNC, error->message);
      return;
    }

  /* Add each device and watch for new devices */
  g_variant_get (reply, "(^a&o)", &object_paths);

  for (unsigned int i = 0; object_paths[i]; i++)
    {
      valent_upower_device_new (object_paths[i],
                                self->cancellable,
                                (GAsyncReadyCallback)device_cb,
                                self);
    }

  g_signal_connect (self->upower,
                    "g-signal",
                    G_CALLBACK (on_upower_signal),
                    self);
}

static void
load_async_cb (GDBusProxy   *proxy,
               GAsyncResult *result,
               gpointer      user_data)
{
  g_autoptr (GTask) task = G_TASK (user_data);
  ValentUPowerDeviceProvider *self = g_task_get_source_object (task);
  GError *error = NULL;

  /* We consider the operation a success if the proxy resolves, then query the devices */
  if ((self->upower = g_dbus_proxy_new_finish (result, &error)) == NULL)
    return g_task_return_error (task, error);

  g_dbus_proxy_call (self->upower,
                     "EnumerateDevices",
                     NULL,
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     self->cancellable,
                     (GAsyncReadyCallback)enumerate_devices_cb,
                     self);

  g_task_return_boolean (task, TRUE);
}

static void
valent_upower_device_provider_load_async (ValentPowerDeviceProvider *provider,
                                          GCancellable              *cancellable,
                                          GAsyncReadyCallback        callback,
                                          gpointer                   user_data)
{
  ValentUPowerDeviceProvider *self = VALENT_UPOWER_DEVICE_PROVIDER (provider);
  g_autoptr (GTask) task = NULL;

  g_assert (VALENT_IS_UPOWER_DEVICE_PROVIDER (provider));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  if (cancellable != NULL)
    g_signal_connect_object (cancellable,
                             "cancelled",
                             G_CALLBACK (g_cancellable_cancel),
                             self->cancellable,
                             G_CONNECT_SWAPPED);

  task = g_task_new (provider, self->cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_upower_device_provider_load_async);

  g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                            G_DBUS_PROXY_FLAGS_NONE,
                            NULL,
                            "org.freedesktop.UPower",
                            "/org/freedesktop/UPower",
                            "org.freedesktop.UPower",
                            self->cancellable,
                            (GAsyncReadyCallback)load_async_cb,
                            g_steal_pointer (&task));
}

/*
 * GObject
 */
static void
valent_upower_device_provider_dispose (GObject *object)
{
  ValentUPowerDeviceProvider *self = VALENT_UPOWER_DEVICE_PROVIDER (object);

  g_cancellable_cancel (self->cancellable);
  g_signal_handlers_disconnect_by_data (self->upower, self);
  g_hash_table_remove_all (self->devices);

  G_OBJECT_CLASS (valent_upower_device_provider_parent_class)->dispose (object);
}

static void
valent_upower_device_provider_finalize (GObject *object)
{
  ValentUPowerDeviceProvider *self = VALENT_UPOWER_DEVICE_PROVIDER (object);

  g_clear_object (&self->cancellable);
  g_clear_object (&self->upower);
  g_clear_pointer (&self->devices, g_hash_table_unref);

  G_OBJECT_CLASS (valent_upower_device_provider_parent_class)->finalize (object);
}

static void
valent_upower_device_provider_class_init (ValentUPowerDeviceProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentPowerDeviceProviderClass *provider_class = VALENT_POWER_DEVICE_PROVIDER_CLASS (klass);

  object_class->dispose = valent_upower_device_provider_dispose;
  object_class->finalize = valent_upower_device_provider_finalize;

  provider_class->load_async = valent_upower_device_provider_load_async;
}

static void
valent_upower_device_provider_init (ValentUPowerDeviceProvider *self)
{
  self->cancellable = g_cancellable_new ();
  self->devices = g_hash_table_new_full (g_str_hash, g_str_equal,
                                         g_free, g_object_unref);
}

