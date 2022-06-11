// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-bluez-device"

#include "config.h"

#include "valent-bluez-device.h"
#include "valent-bluez-profile.h"


struct _ValentBluezDevice
{
  GDBusProxy    parent_instance;

  GCancellable *cancellable;
};

G_DEFINE_TYPE (ValentBluezDevice, valent_bluez_device, G_TYPE_DBUS_PROXY)

enum {
  PROP_0,
  PROP_ADAPTER,
  PROP_ADDRESS,
  PROP_CONNECTED,
  PROP_ICON_NAME,
  PROP_NAME,
  PROP_PAIRED,
  PROP_SERVICES_RESOLVED,
  PROP_TRUSTED,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };


/*
 * GDBusProxy Interface
 */
static void
valent_bluez_device_g_properties_changed (GDBusProxy        *proxy,
                                          GVariant          *changed,
                                          const char *const *invalidated)
{
  ValentBluezDevice *device = VALENT_BLUEZ_DEVICE (proxy);
  const char *key;
  GVariantIter *iter;

  g_variant_get (changed, "a{sv}", &iter);

  while (g_variant_iter_next (iter, "{&sv}", &key, NULL))
    {
      if (g_strcmp0 (key, "Connected") == 0)
        {
          g_object_notify_by_pspec (G_OBJECT (device),
                                    properties [PROP_CONNECTED]);
          continue;
        }

      if (g_strcmp0 (key, "Paired") == 0)
        {
          g_object_notify_by_pspec (G_OBJECT (device),
                                    properties [PROP_PAIRED]);
          continue;
        }

      if (g_strcmp0 (key, "ServicesResolved") == 0)
        {
          g_object_notify_by_pspec (G_OBJECT (device),
                                    properties [PROP_SERVICES_RESOLVED]);
          continue;
        }

      if (g_strcmp0 (key, "Trusted") == 0)
        {
          g_object_notify_by_pspec (G_OBJECT (device),
                                    properties [PROP_TRUSTED]);
          continue;
        }
    }
  g_variant_iter_free (iter);

  for (unsigned int n = 0; invalidated[n] != NULL; n++)
    {
      if (g_strcmp0 (invalidated[n], "SessionId") == 0)
        g_object_notify_by_pspec (G_OBJECT (device), properties [PROP_CONNECTED]);
    }
}

static void
connect_cb (ValentBluezDevice *device,
            GAsyncResult      *res,
            gpointer           user_data)
{
  g_autoptr (GVariant) ret = NULL;
  g_autoptr (GError) error = NULL;

  ret = g_dbus_proxy_call_finish (G_DBUS_PROXY (device), res, &error);

  if (ret != NULL)
    return;

  if (g_dbus_error_is_remote_error (error))
    {
      g_autofree char *name = NULL;

      name = g_dbus_error_get_remote_error (error);

      if (g_strcmp0 (name, "org.bluez.Error.AlreadyConnected") == 0 ||
          g_strcmp0 (name, "org.bluez.Error.InProgress") == 0)
        return;
      else
        g_debug ("[%s] %s", G_STRFUNC, name);
    }

  g_dbus_error_strip_remote_error (error);

  g_warning ("Failed to connect to %s: %s",
             valent_bluez_device_get_name (device),
             error->message);
}

/*
 * GObject
 */
static void
valent_bluez_device_dispose (GObject *object)
{
  ValentBluezDevice *device = VALENT_BLUEZ_DEVICE (object);

  if (!g_cancellable_is_cancelled (device->cancellable))
    g_cancellable_cancel (device->cancellable);

  G_OBJECT_CLASS (valent_bluez_device_parent_class)->dispose (object);
}

static void
valent_bluez_device_finalize (GObject *object)
{
  ValentBluezDevice *device = VALENT_BLUEZ_DEVICE (object);

  g_clear_object (&device->cancellable);

  G_OBJECT_CLASS (valent_bluez_device_parent_class)->finalize (object);
}

static void
valent_bluez_device_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  ValentBluezDevice *self = VALENT_BLUEZ_DEVICE (object);

  switch (prop_id)
    {
    case PROP_ADAPTER:
      g_value_set_string (value, valent_bluez_device_get_adapter (self));
      break;

    case PROP_ADDRESS:
      g_value_set_string (value, valent_bluez_device_get_address (self));
      break;

    case PROP_CONNECTED:
      g_value_set_boolean (value, valent_bluez_device_get_connected (self));
      break;

    case PROP_ICON_NAME:
      g_value_set_string (value, valent_bluez_device_get_icon_name (self));
      break;

    case PROP_NAME:
      g_value_set_string (value, valent_bluez_device_get_name (self));
      break;

    case PROP_PAIRED:
      g_value_set_boolean (value, valent_bluez_device_get_paired (self));
      break;

    case PROP_SERVICES_RESOLVED:
      g_value_set_boolean (value, valent_bluez_device_get_services_resolved (self));
      break;

    case PROP_TRUSTED:
      g_value_set_boolean (value, valent_bluez_device_get_trusted (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_bluez_device_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  ValentBluezDevice *self = VALENT_BLUEZ_DEVICE (object);

  switch (prop_id)
    {
    case PROP_TRUSTED:
      valent_bluez_device_set_trusted (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_bluez_device_init (ValentBluezDevice *device)
{
  device->cancellable = g_cancellable_new ();
}

static void
valent_bluez_device_class_init (ValentBluezDeviceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GDBusProxyClass *proxy_class = G_DBUS_PROXY_CLASS (klass);

  object_class->dispose = valent_bluez_device_dispose;
  object_class->finalize = valent_bluez_device_finalize;
  object_class->get_property = valent_bluez_device_get_property;
  object_class->set_property = valent_bluez_device_set_property;

  proxy_class->g_properties_changed = valent_bluez_device_g_properties_changed;

  /**
   * ValentBluezDevice:adapter
   *
   * The DBus object path of the adapter for this device.
   */
  properties [PROP_ADAPTER] =
    g_param_spec_string ("adapter", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentBluezDevice:address
   *
   * The address of the device.
   */
  properties [PROP_ADDRESS] =
    g_param_spec_string ("address", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentBluezDevice:connected
   *
   * Whether the device is connected.
   */
  properties [PROP_CONNECTED] =
    g_param_spec_boolean ("connected", NULL, NULL,
                          FALSE,
                          (G_PARAM_READABLE |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  /**
   * ValentBluezDevice:icon-name
   *
   * The icon of the device.
   */
  properties [PROP_ICON_NAME] =
    g_param_spec_string ("icon-name", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentBluezDevice:name
   *
   * The name of the device.
   */
  properties [PROP_NAME] =
    g_param_spec_string ("name", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentBluezDevice:paired
   *
   * Whether the device is paired.
   */
  properties [PROP_PAIRED] =
    g_param_spec_boolean ("paired", NULL, NULL,
                          FALSE,
                          (G_PARAM_READABLE |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  /**
   * ValentBluezDevice:services-resolved
   *
   * Whether service discovery has been resolved.
   */
  properties [PROP_SERVICES_RESOLVED] =
    g_param_spec_boolean ("services-resolved", NULL, NULL,
                          FALSE,
                          (G_PARAM_READABLE |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  /**
   * ValentBluezDevice:trusted
   *
   * Whether the device is trusted.
   */
  properties [PROP_TRUSTED] =
    g_param_spec_boolean ("trusted", NULL, NULL,
                          FALSE,
                          (G_PARAM_READABLE |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

/**
 * valent_bluez_device_new:
 * @connection: a #GDBusConnection
 * @object_path: An object path
 * @property_cache: a #GVariant
 * @error: (nullable): a #GError
 *
 * Create a new #ValentBluezDevice on @connection for @object_path. If @properties
 * is not %NULL, the returned proxy will be populated.
 *
 * Returns: (transfer full): a #ValentBluezProxy, or %NULL with @error set
 */
ValentBluezDevice *
valent_bluez_device_new (GDBusConnection  *connection,
                         const char       *object_path,
                         GVariant         *property_cache,
                         GError          **error)
{
  GDBusProxy *proxy;

  g_return_val_if_fail (G_IS_DBUS_CONNECTION (connection), NULL);
  g_return_val_if_fail (g_variant_is_object_path (object_path), NULL);
  g_return_val_if_fail (property_cache == NULL || g_variant_is_of_type (property_cache, G_VARIANT_TYPE ("a{sv}")), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  proxy = g_initable_new (VALENT_TYPE_BLUEZ_DEVICE, NULL, error,
                          "g-connection",     connection,
                          "g-flags",          G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES |
                                              G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
                          "g-name",           "org.bluez",
                          "g-object-path",    object_path,
                          "g-interface-name", "org.bluez.Device1",
                          NULL);

  if (proxy == NULL)
    return NULL;

  if (property_cache != NULL)
    {
      GVariantIter iter;
      const char *name;
      GVariant *value;

      g_variant_iter_init (&iter, property_cache);

      while (g_variant_iter_next (&iter, "{&sv}", &name, &value))
        {
          g_dbus_proxy_set_cached_property (proxy, name, value);
          g_variant_unref (value);
        }
    }

  return VALENT_BLUEZ_DEVICE (proxy);
}

/**
 * valent_bluez_device_get_adapter:
 * @device: a #ValentBluezDevice
 *
 * Get the object path of the adapter the device belongs to.
 *
 * Returns: (transfer none): an object path
 */
const char *
valent_bluez_device_get_adapter (ValentBluezDevice *device)
{
  g_autoptr (GVariant) value = NULL;

  g_return_val_if_fail (VALENT_IS_BLUEZ_DEVICE (device), NULL);

  value = g_dbus_proxy_get_cached_property (G_DBUS_PROXY (device), "Adapter");

  if (value != NULL)
    return g_variant_get_string (value, NULL);

  return NULL;
}

/**
 * valent_bluez_device_get_address:
 * @device: a #ValentBluezDevice
 *
 * Get the Bluetooth device address of the remote device.
 *
 * Returns: (transfer none): a bluetooth address
 */
const char *
valent_bluez_device_get_address (ValentBluezDevice *device)
{
  g_autoptr (GVariant) value = NULL;

  g_return_val_if_fail (VALENT_IS_BLUEZ_DEVICE (device), NULL);

  value = g_dbus_proxy_get_cached_property (G_DBUS_PROXY (device), "Address");

  if (value != NULL)
    return g_variant_get_string (value, NULL);

  return NULL;
}

/**
 * valent_bluez_device_get_connected:
 * @device: A #ValentBluezDevice
 *
 * Get whether the device is connected.
 *
 * Returns: %TRUE if connected
 */
gboolean
valent_bluez_device_get_connected (ValentBluezDevice *device)
{
  g_autoptr (GVariant) value = NULL;

  g_return_val_if_fail (VALENT_IS_BLUEZ_DEVICE (device), FALSE);

  value = g_dbus_proxy_get_cached_property (G_DBUS_PROXY (device), "Connected");

  if (value != NULL)
    return g_variant_get_boolean (value);

  return FALSE;
}

/**
 * valent_bluez_device_get_icon_name:
 * @device: a #ValentBluezDevice
 *
 * Get the themed icon name of @device.
 *
 * Returns: (transfer none): a themed icon name
 */
const char *
valent_bluez_device_get_icon_name (ValentBluezDevice *device)
{
  g_autoptr (GVariant) value = NULL;

  g_return_val_if_fail (VALENT_IS_BLUEZ_DEVICE (device), NULL);

  value = g_dbus_proxy_get_cached_property (G_DBUS_PROXY (device), "Icon");

  if (value != NULL)
    return g_variant_get_string (value, NULL);

  return NULL;
}

/**
 * valent_bluez_device_get_name:
 * @device: a #ValentBluezDevice
 *
 * Get the alias of @device. In case no alias is set, it will return the remote
 * device name.
 *
 * Returns: (transfer none): device name
 */
const char *
valent_bluez_device_get_name (ValentBluezDevice *device)
{
  g_autoptr (GVariant) value = NULL;

  g_return_val_if_fail (VALENT_IS_BLUEZ_DEVICE (device), NULL);

  value = g_dbus_proxy_get_cached_property (G_DBUS_PROXY (device), "Alias");

  if (value != NULL)
    return g_variant_get_string (value, NULL);

  return NULL;
}


/**
 * valent_bluez_device_get_paired:
 * @device: A #ValentBluezDevice
 *
 * Get whether the device is paired.
 *
 * Returns: %TRUE if paired
 */
gboolean
valent_bluez_device_get_paired (ValentBluezDevice *device)
{
  g_autoptr (GVariant) value = NULL;

  g_return_val_if_fail (VALENT_IS_BLUEZ_DEVICE (device), FALSE);

  value = g_dbus_proxy_get_cached_property (G_DBUS_PROXY (device), "Paired");

  if (value != NULL)
    return g_variant_get_boolean (value);

  return FALSE;
}

gboolean
valent_bluez_device_get_services_resolved (ValentBluezDevice *device)
{
  g_autoptr (GVariant) value = NULL;

  g_return_val_if_fail (VALENT_IS_BLUEZ_DEVICE (device), FALSE);

  value = g_dbus_proxy_get_cached_property (G_DBUS_PROXY (device),
                                            "ServicesResolved");

  if (value != NULL)
    return g_variant_get_boolean (value);

  return FALSE;
}

gboolean
valent_bluez_device_get_trusted (ValentBluezDevice *device)
{
  g_autoptr (GVariant) value = NULL;

  g_return_val_if_fail (VALENT_IS_BLUEZ_DEVICE (device), FALSE);

  value = g_dbus_proxy_get_cached_property (G_DBUS_PROXY (device), "Trusted");

  if (value != NULL)
    return g_variant_get_boolean (value);

  return FALSE;
}

void
valent_bluez_device_set_trusted (ValentBluezDevice *device,
                                 gboolean           trusted)
{
  g_return_if_fail (VALENT_IS_BLUEZ_DEVICE (device));

  g_dbus_proxy_call (G_DBUS_PROXY (device),
                     "org.freedesktop.DBus.Properties.Set",
                     g_variant_new ("(ssv)",
                                    "org.bluez.Device1",
                                    "Trusted",
                                    g_variant_new_boolean (trusted)),
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     device->cancellable,
                     NULL,
                     NULL);
}

/**
 * valent_bluez_device_connect:
 * @device: A #ValentBluezDevice
 *
 * Attempt to connect @device.
 */
void
valent_bluez_device_connect (ValentBluezDevice *device)
{
  g_return_if_fail (VALENT_IS_BLUEZ_DEVICE (device));

  g_dbus_proxy_call (G_DBUS_PROXY (device),
                     "ConnectProfile",
                     g_variant_new ("(s)", VALENT_BLUEZ_PROFILE_UUID),
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     device->cancellable,
                     (GAsyncReadyCallback)connect_cb,
                     NULL);
}

/**
 * valent_bluez_device_disconnect:
 * @device: A #ValentBluezDevice
 *
 * Attempt to connect @device.
 */
void
valent_bluez_device_disconnect (ValentBluezDevice *device)
{
  g_return_if_fail (VALENT_IS_BLUEZ_DEVICE (device));

  g_dbus_proxy_call (G_DBUS_PROXY (device),
                     "DisconnectProfile",
                     g_variant_new ("(s)", VALENT_BLUEZ_PROFILE_UUID),
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     device->cancellable,
                     NULL,
                     NULL);
}

/**
 * valent_bluez_device_is_supported:
 * @device: a #ValentBluezDevice
 *
 * Returns %TRUE if @device is paired and the list of service UUIDs includes the
 * KDE Connect bluetooth UUID.
 *
 * Returns: %TRUE if supported
 */
gboolean
valent_bluez_device_is_supported (ValentBluezDevice *device)
{
  g_autoptr (GVariant) paired = NULL;
  g_autoptr (GVariant) value = NULL;
  g_autofree const char **uuids = NULL;

  g_return_val_if_fail (VALENT_IS_BLUEZ_DEVICE (device), FALSE);

  paired = g_dbus_proxy_get_cached_property (G_DBUS_PROXY (device), "Paired");

  if (paired == NULL || !g_variant_get_boolean (paired))
    return FALSE;

  value = g_dbus_proxy_get_cached_property (G_DBUS_PROXY (device), "UUIDs");

  if (value == NULL)
    return FALSE;

  uuids = g_variant_get_strv (value, NULL);

  return g_strv_contains (uuids, VALENT_BLUEZ_PROFILE_UUID);
}

