// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <valent.h>

G_BEGIN_DECLS

#define VALENT_TYPE_CONTACTS_DEVICE (valent_contacts_device_get_type())

G_DECLARE_FINAL_TYPE (ValentContactsDevice, valent_contacts_device, VALENT, CONTACTS_DEVICE, ValentContactsAdapter)

ValentContactsAdapter * valent_contacts_device_new           (ValentDevice          *device);
void                    valent_contacts_device_handle_packet (ValentContactsAdapter *adapter,
                                                              const char            *type,
                                                              JsonNode              *packet);

G_END_DECLS

