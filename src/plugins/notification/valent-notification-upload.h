// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <libvalent-device.h>

G_BEGIN_DECLS

#define VALENT_TYPE_NOTIFICATION_UPLOAD (valent_notification_upload_get_type())

G_DECLARE_FINAL_TYPE (ValentNotificationUpload, valent_notification_upload, VALENT, NOTIFICATION_UPLOAD, ValentTransfer)

ValentTransfer * valent_notification_upload_new (ValentDevice *device,
                                                 JsonNode     *packet,
                                                 GIcon        *icon);

G_END_DECLS

