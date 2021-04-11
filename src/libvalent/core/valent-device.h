// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_CORE_INSIDE) && !defined (VALENT_CORE_COMPILATION)
# error "Only <libvalent-core.h> can be included directly."
#endif

#include "libvalent-core-types.h"

#include <gio/gio.h>
#include <json-glib/json-glib.h>
#include <libpeas/peas.h>

G_BEGIN_DECLS

#define VALENT_TYPE_DEVICE (valent_device_get_type())

G_DECLARE_FINAL_TYPE (ValentDevice, valent_device, VALENT, DEVICE, GObject)

ValentDevice  * valent_device_new               (const char           *id);

/* Properties */
GActionGroup  * valent_device_get_actions       (ValentDevice         *device);
ValentChannel * valent_device_get_channel       (ValentDevice         *device);
gboolean        valent_device_get_connected     (ValentDevice         *device);
ValentData    * valent_device_get_data          (ValentDevice         *device);
const char    * valent_device_get_icon_name     (ValentDevice         *device);
const char    * valent_device_get_id            (ValentDevice         *device);
GMenuModel    * valent_device_get_menu          (ValentDevice         *device);
const char    * valent_device_get_name          (ValentDevice         *device);
gboolean        valent_device_get_paired        (ValentDevice         *device);
GPtrArray     * valent_device_get_plugins       (ValentDevice         *device);

/* Packets */
void            valent_device_queue_packet      (ValentDevice         *device,
                                                 JsonNode             *packet);
void            valent_device_send_packet       (ValentDevice         *device,
                                                 JsonNode             *packet,
                                                 GCancellable         *cancellable,
                                                 GAsyncReadyCallback   callback,
                                                 gpointer              user_data);
gboolean       valent_device_send_packet_finish (ValentDevice         *device,
                                                 GAsyncResult         *result,
                                                 GError              **error);

/* Notifications */
void           valent_device_hide_notification  (ValentDevice         *device,
                                                 const char           *id);
void           valent_device_show_notification  (ValentDevice         *device,
                                                 const char           *id,
                                                 GNotification        *notification);
GFile        * valent_device_new_download_file  (ValentDevice         *device,
                                                 const char           *filename,
                                                 gboolean              unique);

G_END_DECLS

