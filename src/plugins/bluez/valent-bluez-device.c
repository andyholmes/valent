// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-bluez-device"

#include "config.h"

#include <valent.h>

#include "valent-bluez-device.h"
#include "valent-bluez-profile.h"

#define BLUEZ_NAME        "org.bluez"
#define BLUEZ_DEVICE_NAME "org.bluez.Device1"


struct _ValentBluezDevice
{
  GObject          parent_instance;

  GCancellable    *cancellable;
  GDBusConnection *connection;
  char            *object_path;
  unsigned int     properties_changed_id;

  GStrv            uuids;
  gboolean         connected;
  gboolean         paired;
};

G_DEFINE_FINAL_TYPE (ValentBluezDevice, valent_bluez_device, G_TYPE_OBJECT)

typedef enum {
  PROP_CONNECTION = 1,
  PROP_OBJECT_PATH,
} ValentBluezDeviceProperty;

static GParamSpec *properties[PROP_OBJECT_PATH + 1] = { NULL, };


static GWeakRef *
weak_ref_new (GObject *object)
{
  GWeakRef *weak_ref;

  weak_ref = g_new0 (GWeakRef, 1);
  g_weak_ref_init (weak_ref, object);

  return g_steal_pointer (&weak_ref);
}

static void
weak_ref_free (gpointer data)
{
  GWeakRef *weak_ref = data;

  g_weak_ref_clear (weak_ref);
  g_free (weak_ref);
}

static void
on_properties_changed (GDBusConnection *connection,
                       const char      *sender_name,
                       const char      *object_path,
                       const char      *interface_name,
                       const char      *signal_name,
                       GVariant        *parameters,
                       gpointer         user_data)
{
  GWeakRef *proxy_weak = user_data;
  g_autoptr (ValentBluezDevice) self = NULL;
  const char *interface;
  g_autoptr (GVariant) changed = NULL;
  g_autofree char **invalidated = NULL;
  g_autoptr (GVariant) uuids = NULL;

  if ((self = g_weak_ref_get (proxy_weak)) == NULL)
    return;

  if (!g_variant_is_of_type (parameters, G_VARIANT_TYPE ("(sa{sv}as)")))
    {
      g_warning ("Value for PropertiesChanged signal with type '%s' does not match '(sa{sv}as)'",
                 g_variant_get_type_string (parameters));
      return;
    }

  g_variant_get (parameters,
                 "(&s@a{sv}^a&s)",
                 &interface,
                 &changed,
                 &invalidated);

  if (g_strcmp0 (interface, BLUEZ_DEVICE_NAME) != 0)
    return;

  g_variant_lookup (changed, "Connected", "b", &self->connected);
  g_variant_lookup (changed, "Paired", "b", &self->paired);

  uuids = g_variant_lookup_value (changed, "UUIDs", G_VARIANT_TYPE ("as"));

  if (uuids != NULL)
    {
      g_clear_pointer (&self->uuids, g_strfreev);
      self->uuids = g_variant_dup_strv (uuids, NULL);
    }
}

/*
 * GObject
 */
static void
valent_bluez_device_constructed (GObject *object)
{
  ValentBluezDevice *self = VALENT_BLUEZ_DEVICE (object);

  G_OBJECT_CLASS (valent_bluez_device_parent_class)->constructed (object);

  self->properties_changed_id =
    g_dbus_connection_signal_subscribe (self->connection,
                                        BLUEZ_NAME,
                                        "org.freedesktop.DBus.Properties",
                                        "PropertiesChanged",
                                        self->object_path,
                                        BLUEZ_DEVICE_NAME,
                                        G_DBUS_SIGNAL_FLAGS_MATCH_ARG0_NAMESPACE,
                                        on_properties_changed,
                                        weak_ref_new (object),
                                        weak_ref_free);
}

static void
valent_bluez_device_dispose (GObject *object)
{
  ValentBluezDevice *self = VALENT_BLUEZ_DEVICE (object);

  g_cancellable_cancel (self->cancellable);
  if (self->properties_changed_id > 0)
    {
      g_dbus_connection_signal_unsubscribe (self->connection,
                                            self->properties_changed_id);
      self->properties_changed_id = 0;
    }

  G_OBJECT_CLASS (valent_bluez_device_parent_class)->dispose (object);
}

static void
valent_bluez_device_finalize (GObject *object)
{
  ValentBluezDevice *self = VALENT_BLUEZ_DEVICE (object);

  g_clear_object (&self->cancellable);
  g_clear_object (&self->connection);
  g_clear_pointer (&self->object_path, g_free);
  g_clear_pointer (&self->uuids, g_strfreev);

  G_OBJECT_CLASS (valent_bluez_device_parent_class)->finalize (object);
}

static void
valent_bluez_device_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  ValentBluezDevice *self = VALENT_BLUEZ_DEVICE (object);

  switch ((ValentBluezDeviceProperty)prop_id)
    {
    case PROP_CONNECTION:
      g_value_set_object (value, self->connection);
      break;

    case PROP_OBJECT_PATH:
      g_value_set_string (value, self->object_path);
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

  switch ((ValentBluezDeviceProperty)prop_id)
    {
    case PROP_CONNECTION:
      self->connection = g_value_dup_object (value);
      break;

    case PROP_OBJECT_PATH:
      self->object_path = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_bluez_device_class_init (ValentBluezDeviceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = valent_bluez_device_constructed;
  object_class->dispose = valent_bluez_device_dispose;
  object_class->finalize = valent_bluez_device_finalize;
  object_class->get_property = valent_bluez_device_get_property;
  object_class->set_property = valent_bluez_device_set_property;

  /**
   * ValentBluezDevice:connection
   *
   * The D-Bus connection.
   */
  properties [PROP_CONNECTION] =
    g_param_spec_object ("connection", NULL, NULL,
                         G_TYPE_DBUS_CONNECTION,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentBluezDevice:object-path
   *
   * The D-Bus object path of the device.
   */
  properties [PROP_OBJECT_PATH] =
    g_param_spec_string ("object-path", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, G_N_ELEMENTS (properties), properties);
}

static void
valent_bluez_device_init (ValentBluezDevice *device)
{
  device->cancellable = g_cancellable_new ();
}

/**
 * valent_bluez_device_new:
 * @connection: a `GDBusConnection`
 * @object_path: An object path
 * @props: a `GVariant`
 *
 * Create a new `ValentBluezDevice` on @connection for @object_path. If @properties
 * is not %NULL, the returned proxy will be populated.
 *
 * Returns: (transfer full): a new `ValentBluezDevice`
 */
ValentBluezDevice *
valent_bluez_device_new (GDBusConnection  *connection,
                         const char       *object_path,
                         GVariant         *props)
{
  ValentBluezDevice *ret;
  g_autoptr (GVariant) uuids = NULL;

  g_return_val_if_fail (G_IS_DBUS_CONNECTION (connection), NULL);
  g_return_val_if_fail (g_variant_is_object_path (object_path), NULL);
  g_return_val_if_fail (props == NULL || g_variant_is_of_type (props, G_VARIANT_TYPE ("a{sv}")), NULL);

  ret = g_object_new (VALENT_TYPE_BLUEZ_DEVICE,
                      "connection",  connection,
                      "object-path", object_path,
                      NULL);

  g_variant_lookup (props, "Connected", "b", &ret->connected);
  g_variant_lookup (props, "Paired", "b", &ret->paired);

  uuids = g_variant_lookup_value (props, "UUIDs", G_VARIANT_TYPE ("as"));
  if (uuids != NULL)
    ret->uuids = g_variant_dup_strv (uuids, NULL);

  return ret;
}

static void
valent_bluez_device_connect_cb (GDBusConnection   *connection,
                                GAsyncResult      *result,
                                ValentBluezDevice *self)
{
  g_autoptr (GVariant) reply = NULL;
  g_autoptr (GError) error = NULL;

  reply = g_dbus_connection_call_finish (connection, result, &error);
  if (reply != NULL)
    return;

  if (g_dbus_error_is_remote_error (error))
    {
      g_autofree char *remote_error = NULL;

      remote_error = g_dbus_error_get_remote_error (error);
      if (g_strcmp0 (remote_error, "org.bluez.Error.AlreadyConnected") == 0 ||
          g_strcmp0 (remote_error, "org.bluez.Error.InProgress") == 0)
        return;
    }

  if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
      g_dbus_error_strip_remote_error (error);
      g_warning ("Failed to connect to %s: %s",
                 self->object_path,
                 error->message);
    }
}

/**
 * valent_bluez_device_connect:
 * @device: A `ValentBluezDevice`
 *
 * Attempt to connect @device.
 */
void
valent_bluez_device_connect (ValentBluezDevice *device)
{
  g_return_if_fail (VALENT_IS_BLUEZ_DEVICE (device));

  if (!device->paired || device->uuids == NULL)
    return;

  if (!g_strv_contains ((const char * const *)device->uuids,
                        VALENT_BLUEZ_PROFILE_UUID))
    return;

  g_dbus_connection_call (device->connection,
                          BLUEZ_NAME,
                          device->object_path,
                          BLUEZ_DEVICE_NAME,
                          "ConnectProfile",
                          g_variant_new ("(s)", VALENT_BLUEZ_PROFILE_UUID),
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          device->cancellable,
                          (GAsyncReadyCallback)valent_bluez_device_connect_cb,
                          device);
}

