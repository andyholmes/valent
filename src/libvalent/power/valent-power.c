// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-power"

#include "config.h"

#include <libvalent-core.h>

#include "valent-power.h"
#include "valent-power-device.h"
#include "valent-power-device-provider.h"
#include "valent-power-enums.h"


/**
 * SECTION:valent-power
 * @short_description: Power Abstraction
 * @title: ValentPower
 * @stability: Unstable
 * @include: libvalent-power.h
 *
 * #ValentPower is an abstraction of desktop power sources, with a simple API generally intended to
 * be used by #ValentDevicePlugin implementations.
 *
 * The #ValentPower singleton mirrors the default #ValentPowerDevice from the
 * default #ValentPowerDeviceProvider, so should generally be used directly.
 */

struct _ValentPower
{
  ValentComponent    parent_instance;

  GCancellable      *cancellable;

  ValentPowerDevice *default_device;
  GPtrArray         *devices;
};

G_DEFINE_TYPE (ValentPower, valent_power, VALENT_TYPE_COMPONENT)

enum {
  PROP_0,
  PROP_BATTERY_CHARGING,
  PROP_BATTERY_LEVEL,
  PROP_BATTERY_STATE,
  PROP_BATTERY_WARNING,
  N_PROPERTIES,
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

enum {
  CHANGED,
  N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0, };

static ValentPower *default_power = NULL;


static inline void
update_primary_battery (ValentPower *self)
{
  g_assert (VALENT_IS_POWER (self));

  g_clear_object (&self->default_device);

  for (unsigned int i = 0; i < self->devices->len; i++)
    {
      ValentPowerDevice *device = g_ptr_array_index (self->devices, i);

      if (valent_power_device_get_kind (device) == VALENT_POWER_DEVICE_BATTERY)
        {
          VALENT_TRACE_MSG ("New Battery: %s", G_OBJECT_TYPE_NAME (device));
          self->default_device = g_object_ref (device);
          break;
        }
    }
}

/*
 * ValentPowerDeviceProvider Callbacks
 */
static void
on_device_changed (ValentPowerDevice *device,
                   GParamSpec        *pspec,
                   ValentPower       *self)
{
  /* We only propagate changes for the default device */
  if (self->default_device != device)
    return;

  if (g_strcmp0 (g_param_spec_get_name (pspec), "level") == 0)
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_BATTERY_LEVEL]);

  else if (g_strcmp0 (g_param_spec_get_name (pspec), "state") == 0)
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_BATTERY_STATE]);

  else if (g_strcmp0 (g_param_spec_get_name (pspec), "warning") == 0)
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_BATTERY_WARNING]);

  g_signal_emit (G_OBJECT (self), signals [CHANGED], 0);
}

static void
on_device_added (ValentPowerDeviceProvider *provider,
                 ValentPowerDevice         *device,
                 ValentPower               *self)
{
  VALENT_ENTRY;

  g_ptr_array_add (self->devices, g_object_ref (device));
  g_signal_connect_object (device,
                           "notify",
                           G_CALLBACK (on_device_changed),
                           self, 0);

  if (self->default_device == NULL)
    update_primary_battery (self);

  VALENT_EXIT;
}

static void
on_device_removed (ValentPowerDeviceProvider *provider,
                   ValentPowerDevice         *device,
                   ValentPower               *self)
{
  VALENT_ENTRY;

  g_ptr_array_remove (self->devices, device);
  g_signal_handlers_disconnect_by_func (device, on_device_changed, self);

  if (self->default_device == device)
    update_primary_battery (self);

  VALENT_EXIT;
}

static void
valent_power_device_provider_load_cb (ValentPowerDeviceProvider *provider,
                                      GAsyncResult              *result,
                                      ValentPower               *self)
{
  g_autoptr (GError) error = NULL;

  VALENT_ENTRY;

  g_assert (VALENT_IS_POWER_DEVICE_PROVIDER (provider));
  g_assert (VALENT_IS_POWER (self));

  if (!valent_power_device_provider_load_finish (provider, result, &error) &&
      !valent_error_ignore (error))
    g_warning ("%s failed to load: %s", G_OBJECT_TYPE_NAME (provider), error->message);

  VALENT_EXIT;
}

/*
 * ValentComponent
 */
static void
valent_power_provider_added (ValentComponent *component,
                             PeasExtension   *extension)
{
  ValentPower *self = VALENT_POWER (component);
  ValentPowerDeviceProvider *provider = VALENT_POWER_DEVICE_PROVIDER (extension);

  VALENT_ENTRY;

  g_assert (VALENT_IS_POWER (self));
  g_assert (VALENT_IS_POWER_DEVICE_PROVIDER (provider));

  g_signal_connect_object (provider,
                           "device-added",
                           G_CALLBACK (on_device_added),
                           self, 0);

  g_signal_connect_object (provider,
                           "device-removed",
                           G_CALLBACK (on_device_removed),
                           self, 0);

  valent_power_device_provider_load_async (provider,
                                           self->cancellable,
                                           (GAsyncReadyCallback)valent_power_device_provider_load_cb,
                                           self);

  VALENT_EXIT;
}

static void
valent_power_provider_removed (ValentComponent *component,
                               PeasExtension   *extension)
{
  ValentPower *self = VALENT_POWER (component);
  ValentPowerDeviceProvider *provider = VALENT_POWER_DEVICE_PROVIDER (extension);
  g_autoptr (GPtrArray) devices = NULL;

  VALENT_ENTRY;

  g_assert (VALENT_IS_POWER (self));
  g_assert (VALENT_IS_POWER_DEVICE_PROVIDER (provider));

  devices = valent_power_device_provider_get_devices (provider);

  for (unsigned int i = 0; i < devices->len; i++)
    valent_power_device_provider_emit_device_removed (provider, g_ptr_array_index (devices, i));

  g_signal_handlers_disconnect_by_data (provider, self);

  VALENT_EXIT;
}

/*
 * GObject
 */
static void
valent_power_dispose (GObject *object)
{
  ValentPower *self = VALENT_POWER (object);

  if (!g_cancellable_is_cancelled (self->cancellable))
    g_cancellable_cancel (self->cancellable);

  G_OBJECT_CLASS (valent_power_parent_class)->dispose (object);
}

static void
valent_power_finalize (GObject *object)
{
  ValentPower *self = VALENT_POWER (object);

  g_clear_object (&self->cancellable);
  g_clear_object (&self->default_device);
  g_clear_pointer (&self->devices, g_ptr_array_unref);

  G_OBJECT_CLASS (valent_power_parent_class)->finalize (object);
}

static void
valent_power_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  ValentPower *self = VALENT_POWER (object);

  switch (prop_id)
    {
    case PROP_BATTERY_CHARGING:
      g_value_set_boolean (value, valent_power_get_battery_charging (self));
      break;

    case PROP_BATTERY_LEVEL:
      g_value_set_int (value, valent_power_get_battery_level (self));
      break;

    case PROP_BATTERY_STATE:
      g_value_set_enum (value, valent_power_get_battery_state (self));
      break;

    case PROP_BATTERY_WARNING:
      g_value_set_enum (value, valent_power_get_battery_warning (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_power_class_init (ValentPowerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentComponentClass *component_class = VALENT_COMPONENT_CLASS (klass);

  object_class->dispose = valent_power_dispose;
  object_class->finalize = valent_power_finalize;
  object_class->get_property = valent_power_get_property;

  component_class->provider_added = valent_power_provider_added;
  component_class->provider_removed = valent_power_provider_removed;

  /**
   * ValentPower:battery-charging:
   *
   * Whether the primary battery is charging.
   */
  properties [PROP_BATTERY_CHARGING] =
    g_param_spec_boolean ("battery-charging",
                          "Battery Charging",
                          "Whether the primary battery is charging",
                          FALSE,
                          (G_PARAM_READABLE |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  /**
   * ValentPower:battery-level:
   *
   * The current charge level of the primary battery.
   */
  properties [PROP_BATTERY_LEVEL] =
    g_param_spec_int ("battery-level",
                      "Battery Level",
                      "The primary battery level",
                      -1, 100,
                      -1,
                      (G_PARAM_READABLE |
                       G_PARAM_EXPLICIT_NOTIFY |
                       G_PARAM_STATIC_STRINGS));

  /**
   * ValentPower:state:
   *
   * Whether the device is in warning level.
   */
  properties [PROP_BATTERY_STATE] =
    g_param_spec_enum ("battery-state",
                       "Battery State",
                       "The primary battery state",
                       VALENT_TYPE_POWER_STATE,
                       VALENT_POWER_STATE_UNKNOWN,
                       (G_PARAM_READABLE |
                        G_PARAM_EXPLICIT_NOTIFY |
                        G_PARAM_STATIC_STRINGS));

  /**
   * ValentPower:battery-warning:
   *
   * The primary battery's warning level.
   */
  properties [PROP_BATTERY_WARNING] =
    g_param_spec_enum ("battery-warning",
                       "Battery Warning",
                       "The primary battery warning level",
                       VALENT_TYPE_POWER_WARNING,
                       VALENT_POWER_WARNING_NONE,
                       (G_PARAM_READABLE |
                        G_PARAM_EXPLICIT_NOTIFY |
                        G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);

  /**
   * ValentPower::changed:
   * @self: a #ValentPower
   *
   * #ValentPower::changed is emitted when the default power device's properties
   * changes.
   */
  signals [CHANGED] =
    g_signal_new ("changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);
}

static void
valent_power_init (ValentPower *self)
{
  self->cancellable = g_cancellable_new ();
  self->devices = g_ptr_array_new_with_free_func (g_object_unref);
}

/**
 * valent_power_get_default:
 *
 * Get the default #ValentPower.
 *
 * Returns: (transfer none): The default power
 */
ValentPower *
valent_power_get_default (void)
{
  if (default_power == NULL)
    {
      default_power = g_object_new (VALENT_TYPE_POWER,
                                    "plugin-context", "power",
                                    "plugin-type",    VALENT_TYPE_POWER_DEVICE_PROVIDER,
                                    NULL);

      g_object_add_weak_pointer (G_OBJECT (default_power), (gpointer) &default_power);
    }

  return default_power;
}

/**
 * valent_power_get_battery_charging:
 * @power: a #ValentPower
 *
 * Whether the primary battery of @power is charging.
 *
 * Returns: %TRUE if on AC power
 */
gboolean
valent_power_get_battery_charging (ValentPower *power)
{
  g_return_val_if_fail (VALENT_IS_POWER (power), FALSE);

  if G_UNLIKELY (power->default_device == NULL)
    return FALSE;

  return (valent_power_device_get_state (power->default_device) == VALENT_POWER_STATE_CHARGING);
}

/**
 * valent_power_get_battery_level:
 * @power: a #ValentPower
 *
 * Get the charge level of the primary battery backing @power. If no battery is present `-1` is
 * returned.
 *
 * Returns: the current charge level
 */
int
valent_power_get_battery_level (ValentPower *power)
{
  g_return_val_if_fail (VALENT_IS_POWER (power), -1);

  if G_UNLIKELY (power->default_device == NULL)
    return -1;

  return valent_power_device_get_level (power->default_device);
}

/**
 * valent_power_get_battery_state:
 * @power: a #ValentPower
 *
 * Get the charge level of the battery backing @power.
 *
 * Returns: the current charge level
 */
unsigned int
valent_power_get_battery_state (ValentPower *power)
{
  g_return_val_if_fail (VALENT_IS_POWER (power), 0);

  if G_UNLIKELY (power->default_device == NULL)
    return 0;

  return valent_power_device_get_state (power->default_device);
}

/**
 * valent_power_get_battery_warning:
 * @power: a #ValentPower
 *
 * Get the primary battery's warning level.
 *
 * Returns: a #ValentPowerWarning
 */
ValentPowerWarning
valent_power_get_battery_warning (ValentPower *power)
{
  g_return_val_if_fail (VALENT_IS_POWER (power), 0);

  if G_UNLIKELY (power->default_device == NULL)
    return VALENT_POWER_WARNING_NONE;

  return valent_power_device_get_warning (power->default_device);
}

