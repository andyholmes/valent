// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include "valent-device.h"

G_BEGIN_DECLS

#define VALENT_TYPE_DEVICE_IMPL (valent_device_impl_get_type())

G_DECLARE_FINAL_TYPE (ValentDeviceImpl, valent_device_impl, VALENT, DEVICE_IMPL, GDBusInterfaceSkeleton)

GDBusInterfaceSkeleton * valent_device_impl_new (ValentDevice *device);

G_END_DECLS

