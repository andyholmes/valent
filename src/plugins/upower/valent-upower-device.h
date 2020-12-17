// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <libvalent-core.h>
#include <libvalent-power.h>

G_BEGIN_DECLS

#define VALENT_TYPE_UPOWER_DEVICE (valent_upower_device_get_type ())

G_DECLARE_FINAL_TYPE (ValentUPowerDevice, valent_upower_device, VALENT, UPOWER_DEVICE, ValentPowerDevice)

void                valent_upower_device_new        (const char           *object_path,
                                                     GCancellable         *cancellable,
                                                     GAsyncReadyCallback   callback,
                                                     gpointer              user_data);
ValentPowerDevice * valent_upower_device_new_finish (GAsyncResult         *result,
                                                     GError              **error);

G_END_DECLS

