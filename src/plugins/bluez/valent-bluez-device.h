// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <gio/gio.h>

G_BEGIN_DECLS

#define VALENT_TYPE_BLUEZ_DEVICE (valent_bluez_device_get_type())

G_DECLARE_FINAL_TYPE (ValentBluezDevice, valent_bluez_device, VALENT, BLUEZ_DEVICE, GDBusProxy)

ValentBluezDevice * valent_bluez_device_new                   (GDBusConnection    *connection,
                                                               const char         *object_path,
                                                               GVariant           *properties,
                                                               GError            **error);

const char        * valent_bluez_device_get_adapter           (ValentBluezDevice  *device);
const char        * valent_bluez_device_get_address           (ValentBluezDevice  *device);
gboolean            valent_bluez_device_get_connected         (ValentBluezDevice  *device);
const char        * valent_bluez_device_get_icon_name         (ValentBluezDevice  *device);
const char        * valent_bluez_device_get_name              (ValentBluezDevice  *device);
gboolean            valent_bluez_device_get_paired            (ValentBluezDevice  *device);
gboolean            valent_bluez_device_get_services_resolved (ValentBluezDevice  *device);
gboolean            valent_bluez_device_get_trusted           (ValentBluezDevice  *device);
void                valent_bluez_device_set_trusted           (ValentBluezDevice  *device,
                                                               gboolean            trusted);

void                valent_bluez_device_connect               (ValentBluezDevice  *device);
void                valent_bluez_device_disconnect            (ValentBluezDevice  *device);
gboolean            valent_bluez_device_is_supported          (ValentBluezDevice  *device);

G_END_DECLS

