// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_POWER_INSIDE) && !defined (VALENT_POWER_COMPILATION)
# error "Only <libvalent-power.h> can be included directly."
#endif

#include <gio/gio.h>

G_BEGIN_DECLS

/**
 * ValentPowerKind:
 * @VALENT_POWER_DEVICE_UNKNOWN: An unknown device type
 * @VALENT_POWER_DEVICE_LINE_POWER: AC power supply
 * @VALENT_POWER_DEVICE_BATTERY: Battery power supply
 * @VALENT_POWER_DEVICE_UPS: Uninterruptable power supply
 * @VALENT_POWER_DEVICE_MONITOR: A monitor display
 * @VALENT_POWER_DEVICE_MOUSE: A pointer device
 * @VALENT_POWER_DEVICE_KEYBOARD: A keyboard
 * @VALENT_POWER_DEVICE_PDA: A personal digital assistant
 * @VALENT_POWER_DEVICE_PHONE: A mobile phone
 * @VALENT_POWER_DEVICE_MEDIA_PLAYER: A media player
 * @VALENT_POWER_DEVICE_TABLET: A tablet device
 * @VALENT_POWER_DEVICE_COMPUTER: A desktop computer
 * @VALENT_POWER_DEVICE_LAST: Unused
 *
 * Enumeration of power device types, analagous to the #UPowerDeviceKind enumeration.
 */
typedef enum
{
  VALENT_POWER_DEVICE_UNKNOWN,
  VALENT_POWER_DEVICE_LINE_POWER,
  VALENT_POWER_DEVICE_BATTERY,
  VALENT_POWER_DEVICE_UPS,
  VALENT_POWER_DEVICE_MONITOR,
  VALENT_POWER_DEVICE_MOUSE,
  VALENT_POWER_DEVICE_KEYBOARD,
  VALENT_POWER_DEVICE_PDA,
  VALENT_POWER_DEVICE_PHONE,
  VALENT_POWER_DEVICE_MEDIA_PLAYER,
  VALENT_POWER_DEVICE_TABLET,
  VALENT_POWER_DEVICE_COMPUTER,
  VALENT_POWER_DEVICE_LAST
} ValentPowerKind;

/**
 * ValentPowerState:
 * @VALENT_POWER_STATE_UNKNOWN: An unknown device type
 * @VALENT_POWER_STATE_CHARGING: The device is charging
 * @VALENT_POWER_STATE_DISCHARGING: The device is discharging
 * @VALENT_POWER_STATE_ONLINE: The line power is plugged in
 * @VALENT_POWER_STATE_OFFLINE: The line power is unplugged
 * @VALENT_POWER_STATE_RESERVED1: Reserved
 * @VALENT_POWER_STATE_RESERVED2: Reserved
 * @VALENT_POWER_STATE_RESERVED3: Reserved
 *
 * Enumeration of power states.
 */
typedef enum
{
  VALENT_POWER_STATE_UNKNOWN,
  VALENT_POWER_STATE_CHARGING,
  VALENT_POWER_STATE_DISCHARGING,
  VALENT_POWER_STATE_ONLINE,
  VALENT_POWER_STATE_OFFLINE,
  VALENT_POWER_STATE_RESERVED1,
  VALENT_POWER_STATE_RESERVED2,
  VALENT_POWER_STATE_RESERVED3,
} ValentPowerState;

/**
 * ValentPowerWarning:
 * @VALENT_POWER_WARNING_NONE: No warning
 * @VALENT_POWER_WARNING_LOW: Level is low
 * @VALENT_POWER_WARNING_CRITICAL: Level is critically low
 * @VALENT_POWER_WARNING_RESERVED1: Reserved
 * @VALENT_POWER_WARNING_RESERVED2: Reserved
 * @VALENT_POWER_WARNING_RESERVED3: Reserved
 * @VALENT_POWER_WARNING_RESERVED4: Reserved
 * @VALENT_POWER_WARNING_RESERVED5: Reserved
 *
 * Enumeration of power warnging levels.
 */
typedef enum
{
  VALENT_POWER_WARNING_NONE,
  VALENT_POWER_WARNING_LOW,
  VALENT_POWER_WARNING_CRITICAL,
  VALENT_POWER_WARNING_RESERVED1,
  VALENT_POWER_WARNING_RESERVED2,
  VALENT_POWER_WARNING_RESERVED3,
  VALENT_POWER_WARNING_RESERVED4,
  VALENT_POWER_WARNING_RESERVED5
} ValentPowerWarning;


#define VALENT_TYPE_POWER_DEVICE (valent_power_device_get_type())

G_DECLARE_DERIVABLE_TYPE (ValentPowerDevice, valent_power_device, VALENT, POWER_DEVICE, GObject)

struct _ValentPowerDeviceClass
{
  GObjectClass         parent_class;

  ValentPowerKind      (*get_kind)     (ValentPowerDevice *device);
  int                  (*get_level)    (ValentPowerDevice *device);
  ValentPowerState     (*get_state)    (ValentPowerDevice *device);
  ValentPowerWarning   (*get_warning)  (ValentPowerDevice *device);
};

ValentPowerKind      valent_power_device_get_kind     (ValentPowerDevice *device);
int                  valent_power_device_get_level    (ValentPowerDevice *device);
ValentPowerState     valent_power_device_get_state    (ValentPowerDevice *device);
ValentPowerWarning   valent_power_device_get_warning  (ValentPowerDevice *device);


G_END_DECLS

