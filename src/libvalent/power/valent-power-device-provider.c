// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-power-device-provider"

#include "config.h"

#include <libpeas/peas.h>
#include <libvalent-core.h>

#include "valent-power-device.h"
#include "valent-power-device-provider.h"


/**
 * SECTION:valent-power-device-provider
 * @short_description: Base class for power device providers
 * @title: ValentPowerDeviceProvider
 * @stability: Unstable
 * @include: libvalent-power.h
 *
 * #ValentPowerDeviceProvider is base class for plugins that provide #ValentPowerDevice objects.
 */

typedef struct
{
  PeasPluginInfo *plugin_info;

  GPtrArray      *devices;
} ValentPowerDeviceProviderPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (ValentPowerDeviceProvider, valent_power_device_provider, G_TYPE_OBJECT)

/**
 * ValentPowerDeviceProviderClass:
 * @device_added: the class closure for #ValentPowerDeviceProvider::device-added signal
 * @device_removed: the class closure for #ValentPowerDeviceProvider:device-removed signal
 * @load_async: the virtual function pointer for valent_power_device_provider_load_async()
 * @load_finish: the virtual function pointer for valent_power_device_provider_load_finish()
 *
 * The virtual function table for #ValentPowerDeviceProvider.
 */

enum {
  PROP_0,
  PROP_PLUGIN_INFO,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

enum {
  DEVICE_ADDED,
  DEVICE_REMOVED,
  N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0, };


/* LCOV_EXCL_START */
static void
valent_power_device_provider_real_load_async (ValentPowerDeviceProvider *provider,
                                              GCancellable              *cancellable,
                                              GAsyncReadyCallback        callback,
                                              gpointer                   user_data)
{
  g_task_report_new_error (provider, callback, user_data,
                           valent_power_device_provider_real_load_async,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "%s does not implement load_async",
                           G_OBJECT_TYPE_NAME (provider));
}

static gboolean
valent_power_device_provider_real_load_finish (ValentPowerDeviceProvider  *provider,
                                               GAsyncResult               *result,
                                               GError                    **error)
{
  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
valent_power_device_provider_real_device_added (ValentPowerDeviceProvider *provider,
                                                ValentPowerDevice         *device)
{
  ValentPowerDeviceProviderPrivate *priv = valent_power_device_provider_get_instance_private (provider);

  g_assert (VALENT_IS_POWER_DEVICE_PROVIDER (provider));
  g_assert (VALENT_IS_POWER_DEVICE (device));

  if (priv->devices == NULL)
    priv->devices = g_ptr_array_new_with_free_func (g_object_unref);
  g_ptr_array_add (priv->devices, g_object_ref (device));
}

static void
valent_power_device_provider_real_device_removed (ValentPowerDeviceProvider *provider,
                                                  ValentPowerDevice         *device)
{
  ValentPowerDeviceProviderPrivate *priv = valent_power_device_provider_get_instance_private (provider);

  g_assert (VALENT_IS_POWER_DEVICE_PROVIDER (provider));
  g_assert (VALENT_IS_POWER_DEVICE (device));

  /* Maybe we just disposed */
  if (priv->devices == NULL)
    return;

  if (!g_ptr_array_remove (priv->devices, device))
    g_warning ("No such device \"%s\" found in \"%s\"",
               G_OBJECT_TYPE_NAME (device),
               G_OBJECT_TYPE_NAME (provider));
}
/* LCOV_EXCL_STOP */

/*
 * GObject
 */
static void
valent_power_device_provider_dispose (GObject *object)
{
  ValentPowerDeviceProvider *self = VALENT_POWER_DEVICE_PROVIDER (object);
  ValentPowerDeviceProviderPrivate *priv = valent_power_device_provider_get_instance_private (self);

  g_clear_pointer (&priv->devices, g_ptr_array_unref);

  G_OBJECT_CLASS (valent_power_device_provider_parent_class)->dispose (object);
}

static void
valent_power_device_provider_get_property (GObject    *object,
                                           guint       prop_id,
                                           GValue     *value,
                                           GParamSpec *pspec)
{
  ValentPowerDeviceProvider *self = VALENT_POWER_DEVICE_PROVIDER (object);
  ValentPowerDeviceProviderPrivate *priv = valent_power_device_provider_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_PLUGIN_INFO:
      g_value_set_boxed (value, priv->plugin_info);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_power_device_provider_set_property (GObject      *object,
                                           guint         prop_id,
                                           const GValue *value,
                                           GParamSpec   *pspec)
{
  ValentPowerDeviceProvider *self = VALENT_POWER_DEVICE_PROVIDER (object);
  ValentPowerDeviceProviderPrivate *priv = valent_power_device_provider_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_PLUGIN_INFO:
      priv->plugin_info = g_value_get_boxed (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_power_device_provider_class_init (ValentPowerDeviceProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = valent_power_device_provider_dispose;
  object_class->get_property = valent_power_device_provider_get_property;
  object_class->set_property = valent_power_device_provider_set_property;

  klass->device_added = valent_power_device_provider_real_device_added;
  klass->device_removed = valent_power_device_provider_real_device_removed;
  klass->load_async = valent_power_device_provider_real_load_async;
  klass->load_finish = valent_power_device_provider_real_load_finish;

  /**
   * ValentPowerDeviceProvider:plugin-info:
   *
   * The #PeasPluginInfo describing this provider.
   */
  properties [PROP_PLUGIN_INFO] =
    g_param_spec_boxed ("plugin-info",
                        "Plugin Info",
                        "Plugin Info",
                        PEAS_TYPE_PLUGIN_INFO,
                        (G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);

  /**
   * ValentPowerDeviceProvider::device-added:
   * @provider: an #ValentPowerDeviceProvider
   * @device: an #ValentPowerDevice
   *
   * The "device-added" signal is emitted when a provider has discovered a
   * device has become available.
   *
   * Subclasses of #ValentPowerDeviceManager must chain-up if they override the
   * #ValentPowerDeviceProviderClass.device_added vfunc.
   */
  signals [DEVICE_ADDED] =
    g_signal_new ("device-added",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ValentPowerDeviceProviderClass, device_added),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, VALENT_TYPE_POWER_DEVICE);
  g_signal_set_va_marshaller (signals [DEVICE_ADDED],
                              G_TYPE_FROM_CLASS (klass),
                              g_cclosure_marshal_VOID__OBJECTv);

  /**
   * ValentPowerDeviceProvider::device-removed:
   * @provider: an #ValentPowerDeviceProvider
   * @device: an #ValentPowerDevice
   *
   * The "device-removed" signal is emitted when a provider has discovered a
   * device is no longer available.
   *
   * Subclasses of #ValentPowerDeviceManager must chain-up if they override the
   * #ValentPowerDeviceProviderClass.device_removed vfunc.
   */
  signals [DEVICE_REMOVED] =
    g_signal_new ("device-removed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ValentPowerDeviceProviderClass, device_removed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, VALENT_TYPE_POWER_DEVICE);
  g_signal_set_va_marshaller (signals [DEVICE_REMOVED],
                              G_TYPE_FROM_CLASS (klass),
                              g_cclosure_marshal_VOID__OBJECTv);
}

static void
valent_power_device_provider_init (ValentPowerDeviceProvider *provider)
{
}

/**
 * valent_power_device_provider_emit_device_added:
 * @provider: a #ValentPowerDeviceProvider
 * @device: a #ValentPowerDevice
 *
 * Emits the #ValentPowerDeviceProvider::device-added signal.
 *
 * This should only be called by subclasses of #ValentPowerDeviceProvider when
 * a new device has been discovered.
 */
void
valent_power_device_provider_emit_device_added (ValentPowerDeviceProvider *provider,
                                                ValentPowerDevice         *device)
{
  g_return_if_fail (VALENT_IS_POWER_DEVICE_PROVIDER (provider));
  g_return_if_fail (VALENT_IS_POWER_DEVICE (device));

  g_signal_emit (G_OBJECT (provider), signals [DEVICE_ADDED], 0, device);
}

/**
 * valent_power_device_provider_emit_device_removed:
 * @provider: a #ValentPowerDeviceProvider
 * @device: a #ValentPowerDevice
 *
 * Emits the #ValentPowerDeviceProvider::device-removed signal.
 *
 * This should only be called by subclasses of #ValentPowerDeviceProvider when
 * a previously added device has been removed.
 */
void
valent_power_device_provider_emit_device_removed (ValentPowerDeviceProvider *provider,
                                                  ValentPowerDevice         *device)
{
  g_return_if_fail (VALENT_IS_POWER_DEVICE_PROVIDER (provider));
  g_return_if_fail (VALENT_IS_POWER_DEVICE (device));

  g_signal_emit (G_OBJECT (provider), signals [DEVICE_REMOVED], 0, device);
}

/**
 * valent_power_device_provider_get_devices:
 * @provider: an #ValentPowerDeviceProvider
 *
 * Gets a new #GPtrArray containing a list of #ValentPowerDevice instances that were registered by
 * the #ValentPowerDeviceProvider
 *
 * Returns: (transfer container) (element-type Valent.PowerDevice) (not nullable):
 *   a #GPtrArray of #ValentPowerDevice.
 */
GPtrArray *
valent_power_device_provider_get_devices (ValentPowerDeviceProvider *provider)
{
  ValentPowerDeviceProviderPrivate *priv = valent_power_device_provider_get_instance_private (provider);
  g_autoptr (GPtrArray) devices = NULL;

  g_return_val_if_fail (VALENT_IS_POWER_DEVICE_PROVIDER (provider), NULL);

  devices = g_ptr_array_new_with_free_func (g_object_unref);

  if (priv->devices != NULL)
    {
      for (unsigned int i = 0; i < priv->devices->len; i++)
        g_ptr_array_add (devices, g_object_ref (g_ptr_array_index (priv->devices, i)));
    }

  return g_steal_pointer (&devices);
}

/**
 * valent_power_device_provider_load_async:
 * @provider: an #ValentPowerDeviceProvider
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): a #GAsyncReadyCallback
 * @user_data: (closure): user supplied data
 *
 * Requests that the #ValentPowerDeviceProvider asynchronously load any known devices.
 *
 * This should only be called once on an #ValentPowerDeviceProvider. It is an error
 * to call this function more than once for a single #ValentPowerDeviceProvider.
 *
 * #ValentPowerDeviceProvider implementations are expected to emit the
 * #ValentPowerDeviceProvider::power_device-added signal for each power_device they've discovered.
 * That should be done for known devices before returning from the asynchronous
 * operation so that the power_device manager does not need to wait for additional
 * devices to enter the "settled" state.
 */
void
valent_power_device_provider_load_async (ValentPowerDeviceProvider *provider,
                                         GCancellable              *cancellable,
                                         GAsyncReadyCallback        callback,
                                         gpointer                   user_data)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_POWER_DEVICE_PROVIDER (provider));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  VALENT_POWER_DEVICE_PROVIDER_GET_CLASS (provider)->load_async (provider,
                                                                 cancellable,
                                                                 callback,
                                                                 user_data);

  VALENT_EXIT;
}

/**
 * valent_power_device_provider_load_finish:
 * @provider: an #ValentPowerDeviceProvider
 * @result: a #GAsyncResult provided to callback
 * @error: (nullable): a #GError
 *
 * Completes an asynchronous request to load known devices via
 * valent_power_device_provider_load_async().
 *
 * Returns: %TRUE if successful; otherwise %FALSE and @error is set.
 */
gboolean
valent_power_device_provider_load_finish (ValentPowerDeviceProvider  *provider,
                                          GAsyncResult               *result,
                                          GError                    **error)
{
  gboolean ret;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_POWER_DEVICE_PROVIDER (provider), FALSE);
  g_return_val_if_fail (g_task_is_valid (result, provider), FALSE);

  ret =  VALENT_POWER_DEVICE_PROVIDER_GET_CLASS (provider)->load_finish (provider, result, error);

  VALENT_RETURN (ret);
}

