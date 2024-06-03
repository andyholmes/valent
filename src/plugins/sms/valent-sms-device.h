// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <valent.h>

G_BEGIN_DECLS

#define VALENT_TYPE_SMS_DEVICE (valent_sms_device_get_type())

G_DECLARE_FINAL_TYPE (ValentSmsDevice, valent_sms_device, VALENT, SMS_DEVICE, ValentMessagesAdapter)

ValentMessagesAdapter * valent_sms_device_new                    (ValentDevice    *device);
void                 valent_sms_device_handle_messages        (ValentSmsDevice *self,
                                                               JsonNode        *packet);
void                 valent_sms_device_handle_attachment_file (ValentSmsDevice *self,
                                                               JsonNode        *packet);

G_END_DECLS
