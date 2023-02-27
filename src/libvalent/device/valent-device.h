// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_INSIDE) && !defined (VALENT_COMPILATION)
# error "Only <valent.h> can be included directly."
#endif

#include <json-glib/json-glib.h>
#include <libvalent-core.h>

#include "valent-channel.h"

G_BEGIN_DECLS

/**
 * ValentDeviceState:
 * @VALENT_DEVICE_STATE_NONE: Device is unpaired and disconnected
 * @VALENT_DEVICE_STATE_CONNECTED: Device is connected
 * @VALENT_DEVICE_STATE_PAIRED: Device is paired
 * @VALENT_DEVICE_STATE_PAIR_INCOMING: Pair request received from device
 * @VALENT_DEVICE_STATE_PAIR_OUTGOING: Pair request sent to device
 *
 * Device state flags.
 *
 * Since: 1.0
 */
typedef enum
{
  VALENT_DEVICE_STATE_NONE,
  VALENT_DEVICE_STATE_CONNECTED     = (1<<0),
  VALENT_DEVICE_STATE_PAIRED        = (1<<1),
  VALENT_DEVICE_STATE_PAIR_INCOMING = (1<<2),
  VALENT_DEVICE_STATE_PAIR_OUTGOING = (1<<3),
} ValentDeviceState;


#define VALENT_TYPE_DEVICE (valent_device_get_type())

VALENT_AVAILABLE_IN_1_0
G_DECLARE_FINAL_TYPE (ValentDevice, valent_device, VALENT, DEVICE, ValentObject)

VALENT_AVAILABLE_IN_1_0
ValentDevice      * valent_device_new                (const char           *id);
VALENT_AVAILABLE_IN_1_0
ValentChannel     * valent_device_ref_channel        (ValentDevice         *device);
VALENT_AVAILABLE_IN_1_0
ValentData        * valent_device_ref_data           (ValentDevice         *device);
VALENT_AVAILABLE_IN_1_0
const char        * valent_device_get_icon_name      (ValentDevice         *device);
VALENT_AVAILABLE_IN_1_0
const char        * valent_device_get_id             (ValentDevice         *device);
VALENT_AVAILABLE_IN_1_0
GMenuModel        * valent_device_get_menu           (ValentDevice         *device);
VALENT_AVAILABLE_IN_1_0
const char        * valent_device_get_name           (ValentDevice         *device);
VALENT_AVAILABLE_IN_1_0
GStrv               valent_device_get_plugins        (ValentDevice         *device);
VALENT_AVAILABLE_IN_1_0
ValentDeviceState   valent_device_get_state          (ValentDevice         *device);
VALENT_AVAILABLE_IN_1_0
void                valent_device_queue_packet       (ValentDevice         *device,
                                                      JsonNode             *packet);
VALENT_AVAILABLE_IN_1_0
void                valent_device_send_packet        (ValentDevice         *device,
                                                      JsonNode             *packet,
                                                      GCancellable         *cancellable,
                                                      GAsyncReadyCallback   callback,
                                                      gpointer              user_data);
VALENT_AVAILABLE_IN_1_0
gboolean            valent_device_send_packet_finish (ValentDevice         *device,
                                                      GAsyncResult         *result,
                                                      GError              **error);

G_END_DECLS

