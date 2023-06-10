// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <valent.h>

G_BEGIN_DECLS

#define VALENT_TYPE_MOUSEPAD_DEVICE (valent_mousepad_device_get_type())

G_DECLARE_FINAL_TYPE (ValentMousepadDevice, valent_mousepad_device, VALENT, MOUSEPAD_DEVICE, ValentMediaPlayer)

ValentMousepadDevice * valent_mousepad_device_new           (ValentDevice         *device);
void                   valent_mousepad_device_handle_packet (ValentMousepadDevice *player,
                                                             JsonNode             *packet);

G_END_DECLS

