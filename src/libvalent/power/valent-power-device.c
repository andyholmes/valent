// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-power-device"

#include "config.h"

#include <gio/gio.h>

#include "valent-power-device.h"
#include "valent-power-enums.h"


/**
 * SECTION:valent-power-device
 * @short_description: Base class for power devices
 * @title: ValentPowerDevice
 * @stability: Unstable
 * @include: libvalent-power.h
 *
 * #ValentPowerDevice is a base class for power devices, such as laptop batteries.
 */

G_DEFINE_TYPE (ValentPowerDevice, valent_power_device, G_TYPE_OBJECT)

/**
 * ValentPowerDeviceClass:
 * @get_kind: the virtual function pointer for valent_power_device_get_kind()
 * @get_level: the virtual function pointer for valent_power_device_get_level()
 * @get_state: the virtual function pointer for valent_power_device_get_state()
 * @get_warning: the virtual function pointer for valent_power_device_get_warning()
 *
 * The virtual function table for #ValentPowerDevice.
 */

enum {
  PROP_0,
  PROP_KIND,
  PROP_LEVEL,
  PROP_STATE,
  PROP_WARNING,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };


/* LCOV_EXCL_START */
static ValentPowerKind
valent_power_device_real_get_kind (ValentPowerDevice *device)
{
  return VALENT_POWER_DEVICE_UNKNOWN;
}

static int
valent_power_device_real_get_level (ValentPowerDevice *device)
{
  return -1;
}

static ValentPowerState
valent_power_device_real_get_state (ValentPowerDevice *device)
{
  return VALENT_POWER_STATE_UNKNOWN;
}

static ValentPowerWarning
valent_power_device_real_get_warning (ValentPowerDevice *device)
{
  return VALENT_POWER_WARNING_NONE;
}
/* LCOV_EXCL_STOP */

/*
 * GObject
 */
static void
valent_power_device_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  ValentPowerDevice *self = VALENT_POWER_DEVICE (object);

  switch (prop_id)
    {
    case PROP_KIND:
      g_value_set_enum (value, valent_power_device_get_kind (self));
      break;

    case PROP_LEVEL:
      g_value_set_int (value, valent_power_device_get_level (self));
      break;

    case PROP_STATE:
      g_value_set_enum (value, valent_power_device_get_state (self));
      break;

    case PROP_WARNING:
      g_value_set_enum (value, valent_power_device_get_warning (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_power_device_class_init (ValentPowerDeviceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentPowerDeviceClass *device_class = VALENT_POWER_DEVICE_CLASS (klass);

  object_class->get_property = valent_power_device_get_property;

  device_class->get_kind = valent_power_device_real_get_kind;
  device_class->get_level = valent_power_device_real_get_level;
  device_class->get_state = valent_power_device_real_get_state;
  device_class->get_warning = valent_power_device_real_get_warning;

  /**
   * ValentPowerDevice:kind:
   *
   * The device type.
   */
  properties [PROP_KIND] =
    g_param_spec_enum ("kind",
                       "Kind",
                       "The device type",
                       VALENT_TYPE_POWER_KIND,
                       VALENT_POWER_DEVICE_UNKNOWN,
                       (G_PARAM_READABLE |
                        G_PARAM_EXPLICIT_NOTIFY |
                        G_PARAM_STATIC_STRINGS));

  /**
   * ValentPowerDevice:level:
   *
   * The current charge level.
   */
  properties [PROP_LEVEL] =
    g_param_spec_int ("level",
                      "Level",
                      "Power Level",
                      -1, 100,
                      -1,
                      (G_PARAM_READABLE |
                       G_PARAM_EXPLICIT_NOTIFY |
                       G_PARAM_STATIC_STRINGS));

  /**
   * ValentPowerDevice:state:
   *
   * The state of the device.
   */
  properties [PROP_STATE] =
    g_param_spec_enum ("state",
                       "State",
                       "The device state",
                       VALENT_TYPE_POWER_STATE,
                       VALENT_POWER_STATE_UNKNOWN,
                       (G_PARAM_READABLE |
                        G_PARAM_EXPLICIT_NOTIFY |
                        G_PARAM_STATIC_STRINGS));

  /**
   * ValentPowerDevice:warning:
   *
   * The warning level.
   */
  properties [PROP_WARNING] =
    g_param_spec_enum ("warning",
                       "Warning",
                       "The warning level",
                       VALENT_TYPE_POWER_WARNING,
                       VALENT_POWER_WARNING_NONE,
                       (G_PARAM_READABLE |
                        G_PARAM_EXPLICIT_NOTIFY |
                        G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
valent_power_device_init (ValentPowerDevice *self)
{
}

/**
 * valent_power_device_get_kind:
 * @device: a #ValentPowerDevice
 *
 * Get the type of @device. If the result is not %VALENT_POWER_DEVICE_BATTERY,
 * valent_power_device_get_battery_level() will return `-1`.
 *
 * Returns: a #ValentPowerKind
 */
ValentPowerKind
valent_power_device_get_kind (ValentPowerDevice *device)
{
  g_return_val_if_fail (VALENT_IS_POWER_DEVICE (device), -1);

  return VALENT_POWER_DEVICE_GET_CLASS (device)->get_kind (device);
}

/**
 * valent_power_device_get_level:
 * @device: a #ValentPowerDevice
 *
 * Get the charge level of @device. If the result is `-1` the battery for @device is offline or
 * missing.
 *
 * Returns: a charge level
 */
int
valent_power_device_get_level (ValentPowerDevice *device)
{
  g_return_val_if_fail (VALENT_IS_POWER_DEVICE (device), -1);

  return VALENT_POWER_DEVICE_GET_CLASS (device)->get_level (device);
}

/**
 * valent_power_device_get_state:
 * @device: a #ValentPowerDevice
 *
 * Get the state for @device. If #ValentPowerDevice:kind is %VALENT_POWER_DEVICE_BATTERY this will
 * return either %VALENT_POWER_STATE_CHARGING or %VALENT_POWER_STATE_DISCHARGING.
 *
 * Returns: a #ValentPowerState
 */
ValentPowerState
valent_power_device_get_state (ValentPowerDevice *device)
{
  g_return_val_if_fail (VALENT_IS_POWER_DEVICE (device), FALSE);

  return VALENT_POWER_DEVICE_GET_CLASS (device)->get_state (device);
}

/**
 * valent_power_device_get_warning:
 * @device: a #ValentPowerDevice
 *
 * Get the warning level for @device. If #ValentPowerDevice:kind is not
 * %VALENT_POWER_DEVICE_BATTERY the will always return %VALENT_POWER_WARNING_NONE.
 *
 * Returns: a #ValentPowerWarning
 */
ValentPowerWarning
valent_power_device_get_warning (ValentPowerDevice *device)
{
  g_return_val_if_fail (VALENT_IS_POWER_DEVICE (device), FALSE);

  return VALENT_POWER_DEVICE_GET_CLASS (device)->get_warning (device);
}

