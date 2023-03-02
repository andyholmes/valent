// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <gio/gio.h>

G_BEGIN_DECLS

#define VALENT_TYPE_BLUEZ_DEVICE (valent_bluez_device_get_type())

G_DECLARE_FINAL_TYPE (ValentBluezDevice, valent_bluez_device, VALENT, BLUEZ_DEVICE, GObject)

ValentBluezDevice * valent_bluez_device_new          (GDBusConnection   *connection,
                                                      const char        *object_path,
                                                      GVariant          *props);
void                valent_bluez_device_connect      (ValentBluezDevice *device);
gboolean            valent_bluez_device_is_supported (ValentBluezDevice *device);

G_END_DECLS

