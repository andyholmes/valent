// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-upower-device"

#include "config.h"

#include <gio/gio.h>
#include <libvalent-core.h>
#include <libvalent-power.h>

#include "valent-upower-device.h"


struct _ValentUPowerDevice
{
  ValentPowerDevice   parent_instance;

  GDBusProxy         *proxy;
  char               *object_path;

  gboolean            charging;
  unsigned int        kind;
  int                 level;
  unsigned int        state;
  ValentPowerWarning  warning;
};


static void async_initable_iface_init (GAsyncInitableIface *iface);

G_DEFINE_TYPE_WITH_CODE (ValentUPowerDevice, valent_upower_device, VALENT_TYPE_POWER_DEVICE,
                         G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE, async_initable_iface_init))

enum {
  PROP_0,
  PROP_OBJECT_PATH,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

/*
 * UPower Enums
 */
enum {
  STATE_UNKNOWN,
  STATE_CHARGING,
  STATE_DISCHARGING,
  STATE_EMPTY,
  STATE_FULLY_CHARGED,
  STATE_PENDING_CHARGE,
  STATE_PENDING_DISCHARGE,
  STATE_LAST
};

enum {
  LEVEL_UNKNOWN,
  LEVEL_NONE,
  LEVEL_DISCHARGING,
  LEVEL_LOW,
  LEVEL_CRITICAL,
  LEVEL_ACTION,
  LEVEL_LAST
};

static void
handle_online (ValentUPowerDevice *self,
               gboolean            online)
{
  guint32 state;

  if (self->kind != VALENT_POWER_DEVICE_LINE_POWER)
    return;

  state = online ? VALENT_POWER_STATE_ONLINE : VALENT_POWER_STATE_OFFLINE;

  if (self->state == state)
    return;

  self->state = state;
  g_object_notify (G_OBJECT (self), "state");
}

static void
handle_percentage (ValentUPowerDevice *self,
                   double              percentage)
{
  int level;

  level = percentage;

  if (self->level == level)
    return;

  self->level = level;
  g_object_notify (G_OBJECT (self), "level");
}

static void
handle_state (ValentUPowerDevice *self,
              guint32             state)
{
  /* We only use this property for battery types */
  if (self->kind != VALENT_POWER_DEVICE_BATTERY)
    return;

  if (state == STATE_DISCHARGING)
    state = VALENT_POWER_STATE_DISCHARGING;
  else
    state = VALENT_POWER_STATE_CHARGING;

  if (self->state == state)
    return;

  self->state = state;
  g_object_notify (G_OBJECT (self), "state");
}

static void
handle_warning (ValentUPowerDevice *self,
                guint32             warning)
{
  // TODO: different threshold events
  if (self->charging && warning >= LEVEL_LOW)
    warning = VALENT_POWER_WARNING_LOW;
  else
    warning = VALENT_POWER_WARNING_NONE;

  if (self->warning == warning)
    return;

  self->warning = warning;
  g_object_notify (G_OBJECT (self), "warning");
}

static void
init_properties (ValentUPowerDevice *self)
{
  g_autoptr (GVariant) type = NULL;
  g_autoptr (GVariant) online = NULL;
  g_autoptr (GVariant) percentage = NULL;
  g_autoptr (GVariant) state = NULL;
  g_autoptr (GVariant) warning = NULL;

  /* First check the device type */
  if ((type = g_dbus_proxy_get_cached_property (self->proxy, "Type")) != NULL)
    self->kind = g_variant_get_uint32 (type);

  if (self->kind == VALENT_POWER_DEVICE_BATTERY)
    {
      /* Battery Level */
      if ((percentage = g_dbus_proxy_get_cached_property (self->proxy, "Percentage")) != NULL)
        handle_percentage (self, g_variant_get_double (percentage));

      /* Charging */
      if ((state = g_dbus_proxy_get_cached_property (self->proxy, "State")) != NULL)
        handle_state (self, g_variant_get_uint32 (state));

      /* Threshold FIXME */
      if ((warning = g_dbus_proxy_get_cached_property (self->proxy, "WarningLevel")) != NULL)
        handle_warning (self, g_variant_get_uint32 (warning));
    }
  else if (self->kind == VALENT_POWER_DEVICE_LINE_POWER)
    {
      if ((online = g_dbus_proxy_get_cached_property (self->proxy, "Online")) != NULL)
        handle_online (self, g_variant_get_boolean (online));
    }
}

static void
on_properties_changed (GDBusProxy         *proxy,
                       GVariant           *changed_properties,
                       const char         *invalidated_properties,
                       ValentUPowerDevice *self)
{
  gboolean is_present;
  gboolean online;
  double percentage;
  guint32 state;
  guint32 warning;

  g_assert (VALENT_IS_UPOWER_DEVICE (self));

  if (g_variant_lookup (changed_properties, "IsPresent", "b", &is_present))
    return init_properties (self);

  if (g_variant_lookup (changed_properties, "Online", "b", &online))
    handle_online (self, online);

  if (g_variant_lookup (changed_properties, "Percentage", "d", &percentage))
    handle_percentage (self, percentage);

  if (g_variant_lookup (changed_properties, "State", "u", &state))
    handle_state (self, state);

  if (g_variant_lookup (changed_properties, "WarningLevel", "u", &warning))
    handle_warning (self, warning);
}

/*
 * GAsyncInitable
 */
static void
valent_upower_device_init_cb (GObject      *object,
                              GAsyncResult *result,
                              gpointer      user_data)
{
  g_autoptr (GTask) task = G_TASK (user_data);
  ValentUPowerDevice *self = g_task_get_source_object (task);
  GError *error = NULL;

  g_assert (VALENT_IS_UPOWER_DEVICE (self));
  g_assert (g_variant_is_object_path (self->object_path));

  self->proxy = g_dbus_proxy_new_finish (result, &error);

  if (self->proxy == NULL)
    return g_task_return_error (task, error);

  init_properties (self);

  g_signal_connect (self->proxy,
                    "g-properties-changed",
                    G_CALLBACK (on_properties_changed),
                    self);

  g_task_return_boolean (task, TRUE);
}

static void
valent_upower_device_init_async (GAsyncInitable      *initable,
                                 gint                 io_priority,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
  ValentUPowerDevice *self = VALENT_UPOWER_DEVICE (initable);
  g_autoptr (GTask) task = NULL;

  g_assert (VALENT_IS_UPOWER_DEVICE (self));
  g_assert (g_variant_is_object_path (self->object_path));

  task = g_task_new (initable, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_upower_device_init_async);

  g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                            G_DBUS_PROXY_FLAGS_GET_INVALIDATED_PROPERTIES,
                            NULL,
                            "org.freedesktop.UPower",
                            self->object_path,
                            "org.freedesktop.UPower.Device",
                            cancellable,
                            valent_upower_device_init_cb,
                            g_steal_pointer (&task));
}

static gboolean
valent_upower_device_init_finish (GAsyncInitable  *initable,
                                  GAsyncResult    *result,
                                  GError         **error)
{
  g_return_val_if_fail (g_task_is_valid (result, initable), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
async_initable_iface_init (GAsyncInitableIface *iface)
{
  iface->init_async = valent_upower_device_init_async;
  iface->init_finish = valent_upower_device_init_finish;
}

/*
 * ValentPowerDevice
 */
static ValentPowerKind
valent_upower_device_get_kind (ValentPowerDevice *device)
{
  ValentUPowerDevice *self = VALENT_UPOWER_DEVICE (device);

  g_assert (VALENT_IS_UPOWER_DEVICE (self));

  return self->kind;
}

static int
valent_upower_device_get_level (ValentPowerDevice *device)
{
  ValentUPowerDevice *self = VALENT_UPOWER_DEVICE (device);

  g_assert (VALENT_IS_UPOWER_DEVICE (self));

  return self->level;
}

static ValentPowerState
valent_upower_device_get_state (ValentPowerDevice *device)
{
  ValentUPowerDevice *self = VALENT_UPOWER_DEVICE (device);

  g_assert (VALENT_IS_UPOWER_DEVICE (self));

  return self->state;
}

static ValentPowerWarning
valent_upower_device_get_warning (ValentPowerDevice *device)
{
  ValentUPowerDevice *self = VALENT_UPOWER_DEVICE (device);

  g_assert (VALENT_IS_UPOWER_DEVICE (self));

  return self->warning;
}

/*
 * GObject
 */
static void
valent_upower_device_finalize (GObject *object)
{
  ValentUPowerDevice *self = VALENT_UPOWER_DEVICE (object);

  g_signal_handlers_disconnect_by_func (self->proxy, on_properties_changed, self);
  g_clear_object (&self->proxy);

  G_OBJECT_CLASS (valent_upower_device_parent_class)->finalize (object);
}

static void
valent_upower_device_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  ValentUPowerDevice *self = VALENT_UPOWER_DEVICE (object);

  switch (prop_id)
    {
    case PROP_OBJECT_PATH:
      g_value_set_string (value, self->object_path);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_upower_device_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  ValentUPowerDevice *self = VALENT_UPOWER_DEVICE (object);

  switch (prop_id)
    {
    case PROP_OBJECT_PATH:
      self->object_path = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_upower_device_class_init (ValentUPowerDeviceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentPowerDeviceClass *device_class = VALENT_POWER_DEVICE_CLASS (klass);

  object_class->finalize = valent_upower_device_finalize;
  object_class->get_property = valent_upower_device_get_property;
  object_class->set_property = valent_upower_device_set_property;

  device_class->get_kind = valent_upower_device_get_kind;
  device_class->get_level = valent_upower_device_get_level;
  device_class->get_state = valent_upower_device_get_state;
  device_class->get_warning = valent_upower_device_get_warning;

  /**
   * ValentUPowerDevice:object-path:
   *
   * The object path the device is exported on.
   */
  properties [PROP_OBJECT_PATH] =
    g_param_spec_string ("object-path",
                         "Object Path",
                         "The object path the device is on.",
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
valent_upower_device_init (ValentUPowerDevice *self)
{
}

void
valent_upower_device_new (const char          *object_path,
                          GCancellable        *cancellable,
                          GAsyncReadyCallback  callback,
                          gpointer             user_data)
{
  g_async_initable_new_async (VALENT_TYPE_UPOWER_DEVICE,
                              G_PRIORITY_DEFAULT,
                              cancellable,
                              callback,
                              user_data,
                              "object-path", object_path,
                              NULL);
}

ValentPowerDevice *
valent_upower_device_new_finish (GAsyncResult  *result,
                                 GError       **error)
{
  GObject *ret;
  g_autoptr (GObject) source_object = NULL;

  source_object = g_async_result_get_source_object (result);
  ret = g_async_initable_new_finish (G_ASYNC_INITABLE (source_object),
                                     result,
                                     error);

  if (ret != NULL)
    return VALENT_POWER_DEVICE (ret);

  return NULL;
}

