// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-test-power-device"

#include "config.h"

#include <gio/gio.h>
#include <libvalent-core.h>
#include <libvalent-power.h>

#include "valent-test-power-device.h"


struct _ValentTestPowerDevice
{
  ValentPowerDevice   parent_instance;

  ValentPowerKind     kind;
  int                 level;
  ValentPowerState    state;
  ValentPowerWarning  warning;
};

G_DEFINE_TYPE (ValentTestPowerDevice, valent_test_power_device, VALENT_TYPE_POWER_DEVICE)


/*
 * ValentPowerDevice
 */
static ValentPowerKind
valent_test_power_device_get_kind (ValentPowerDevice *device)
{
  ValentTestPowerDevice *self = VALENT_TEST_POWER_DEVICE (device);

  g_assert (VALENT_IS_TEST_POWER_DEVICE (self));

  return self->kind;
}

static int
valent_test_power_device_get_level (ValentPowerDevice *device)
{
  ValentTestPowerDevice *self = VALENT_TEST_POWER_DEVICE (device);

  g_assert (VALENT_IS_TEST_POWER_DEVICE (self));

  return self->level;
}

static ValentPowerState
valent_test_power_device_get_state (ValentPowerDevice *device)
{
  ValentTestPowerDevice *self = VALENT_TEST_POWER_DEVICE (device);

  g_assert (VALENT_IS_TEST_POWER_DEVICE (self));

  return self->state;
}

static ValentPowerWarning
valent_test_power_device_get_warning (ValentPowerDevice *device)
{
  ValentTestPowerDevice *self = VALENT_TEST_POWER_DEVICE (device);

  g_assert (VALENT_IS_TEST_POWER_DEVICE (self));

  return self->warning;
}

/*
 * GObject
 */
static void
valent_test_power_device_class_init (ValentTestPowerDeviceClass *klass)
{
  ValentPowerDeviceClass *device_class = VALENT_POWER_DEVICE_CLASS (klass);

  device_class->get_kind = valent_test_power_device_get_kind;
  device_class->get_level = valent_test_power_device_get_level;
  device_class->get_state = valent_test_power_device_get_state;
  device_class->get_warning = valent_test_power_device_get_warning;
}

static void
valent_test_power_device_init (ValentTestPowerDevice *self)
{
  self->kind = VALENT_POWER_DEVICE_UNKNOWN;
  self->level = -1;
  self->state = VALENT_POWER_STATE_UNKNOWN;
  self->warning = VALENT_POWER_WARNING_NONE;
}

/**
 * valent_test_power_device_set_kind:
 * @self: a #ValentTestPowerDevice
 * @kind: a #ValentPowerKind
 *
 * Set the device kind of @self.
 */
void
valent_test_power_device_set_kind (ValentTestPowerDevice *self,
                                   ValentPowerKind        kind)
{
  g_assert (VALENT_IS_TEST_POWER_DEVICE (self));

  if (self->kind == kind)
    return;

  self->kind = kind;
  g_object_notify (G_OBJECT (self), "kind");
}

/**
 * valent_test_power_device_set_level:
 * @self: a #ValentTestPowerDevice
 * @level: the charge level
 *
 * Set the charge level of @self.
 */
void
valent_test_power_device_set_level (ValentTestPowerDevice *self,
                                    int                    level)
{
  g_assert (VALENT_IS_TEST_POWER_DEVICE (self));

  if (self->level == level)
    return;

  self->level = level;
  g_object_notify (G_OBJECT (self), "level");
}

/**
 * valent_test_power_device_set_state:
 * @self: a #ValentTestPowerDevice
 * @state: whether the device is charging or not
 *
 * Set the state of @self.
 */
void
valent_test_power_device_set_state (ValentTestPowerDevice *self,
                                    unsigned int           state)
{
  g_assert (VALENT_IS_TEST_POWER_DEVICE (self));

  if (self->state == state)
    return;

  self->state = state;
  g_object_notify (G_OBJECT (self), "state");
}

/**
 * valent_test_power_device_set_warning:
 * @self: a #ValentTestPowerDevice
 * @warning: a #ValentPowerWarning
 *
 * Set the warning level of @self.
 */
void
valent_test_power_device_set_warning (ValentTestPowerDevice *self,
                                      ValentPowerWarning     warning)
{
  g_assert (VALENT_IS_TEST_POWER_DEVICE (self));

  if (self->warning == warning)
    return;

  self->warning = warning;
  g_object_notify (G_OBJECT (self), "warning");
}

