// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-mock-power-device"

#include "config.h"

#include <gio/gio.h>
#include <libvalent-core.h>
#include <libvalent-power.h>

#include "valent-mock-power-device.h"


struct _ValentMockPowerDevice
{
  ValentPowerDevice   parent_instance;

  ValentPowerKind     kind;
  int                 level;
  ValentPowerState    state;
  ValentPowerWarning  warning;
};

G_DEFINE_TYPE (ValentMockPowerDevice, valent_mock_power_device, VALENT_TYPE_POWER_DEVICE)


/*
 * ValentPowerDevice
 */
static ValentPowerKind
valent_mock_power_device_get_kind (ValentPowerDevice *device)
{
  ValentMockPowerDevice *self = VALENT_MOCK_POWER_DEVICE (device);

  g_assert (VALENT_IS_MOCK_POWER_DEVICE (self));

  return self->kind;
}

static int
valent_mock_power_device_get_level (ValentPowerDevice *device)
{
  ValentMockPowerDevice *self = VALENT_MOCK_POWER_DEVICE (device);

  g_assert (VALENT_IS_MOCK_POWER_DEVICE (self));

  return self->level;
}

static ValentPowerState
valent_mock_power_device_get_state (ValentPowerDevice *device)
{
  ValentMockPowerDevice *self = VALENT_MOCK_POWER_DEVICE (device);

  g_assert (VALENT_IS_MOCK_POWER_DEVICE (self));

  return self->state;
}

static ValentPowerWarning
valent_mock_power_device_get_warning (ValentPowerDevice *device)
{
  ValentMockPowerDevice *self = VALENT_MOCK_POWER_DEVICE (device);

  g_assert (VALENT_IS_MOCK_POWER_DEVICE (self));

  return self->warning;
}

/*
 * GObject
 */
static void
valent_mock_power_device_class_init (ValentMockPowerDeviceClass *klass)
{
  ValentPowerDeviceClass *device_class = VALENT_POWER_DEVICE_CLASS (klass);

  device_class->get_kind = valent_mock_power_device_get_kind;
  device_class->get_level = valent_mock_power_device_get_level;
  device_class->get_state = valent_mock_power_device_get_state;
  device_class->get_warning = valent_mock_power_device_get_warning;
}

static void
valent_mock_power_device_init (ValentMockPowerDevice *self)
{
  self->kind = VALENT_POWER_DEVICE_UNKNOWN;
  self->level = -1;
  self->state = VALENT_POWER_STATE_UNKNOWN;
  self->warning = VALENT_POWER_WARNING_NONE;
}

/**
 * valent_mock_power_device_set_kind:
 * @self: a #ValentMockPowerDevice
 * @kind: a #ValentPowerKind
 *
 * Set the device kind of @self.
 */
void
valent_mock_power_device_set_kind (ValentMockPowerDevice *self,
                                   ValentPowerKind        kind)
{
  g_assert (VALENT_IS_MOCK_POWER_DEVICE (self));

  if (self->kind == kind)
    return;

  self->kind = kind;
  g_object_notify (G_OBJECT (self), "kind");
}

/**
 * valent_mock_power_device_set_level:
 * @self: a #ValentMockPowerDevice
 * @level: the charge level
 *
 * Set the charge level of @self.
 */
void
valent_mock_power_device_set_level (ValentMockPowerDevice *self,
                                    int                    level)
{
  g_assert (VALENT_IS_MOCK_POWER_DEVICE (self));

  if (self->level == level)
    return;

  self->level = level;
  g_object_notify (G_OBJECT (self), "level");
}

/**
 * valent_mock_power_device_set_state:
 * @self: a #ValentMockPowerDevice
 * @state: whether the device is charging or not
 *
 * Set the state of @self.
 */
void
valent_mock_power_device_set_state (ValentMockPowerDevice *self,
                                    unsigned int           state)
{
  g_assert (VALENT_IS_MOCK_POWER_DEVICE (self));

  if (self->state == state)
    return;

  self->state = state;
  g_object_notify (G_OBJECT (self), "state");
}

/**
 * valent_mock_power_device_set_warning:
 * @self: a #ValentMockPowerDevice
 * @warning: a #ValentPowerWarning
 *
 * Set the warning level of @self.
 */
void
valent_mock_power_device_set_warning (ValentMockPowerDevice *self,
                                      ValentPowerWarning     warning)
{
  g_assert (VALENT_IS_MOCK_POWER_DEVICE (self));

  if (self->warning == warning)
    return;

  self->warning = warning;
  g_object_notify (G_OBJECT (self), "warning");
}

