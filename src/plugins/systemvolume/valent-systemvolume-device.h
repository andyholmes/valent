// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <valent.h>

G_BEGIN_DECLS

#define VALENT_TYPE_SYSTEMVOLUME_DEVICE (valent_systemvolume_device_get_type())

G_DECLARE_FINAL_TYPE (ValentSystemvolumeDevice, valent_systemvolume_device, VALENT, SYSTEMVOLUME_DEVICE, ValentMixerAdapter)

ValentMixerAdapter * valent_systemvolume_device_new           (ValentDevice             *device);
void                 valent_systemvolume_device_handle_packet (ValentSystemvolumeDevice *self,
                                                               JsonNode                 *packet);

G_END_DECLS
