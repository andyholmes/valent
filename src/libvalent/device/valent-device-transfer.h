// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_INSIDE) && !defined (VALENT_COMPILATION)
# error "Only <valent.h> can be included directly."
#endif

#include <gio/gio.h>
#include <json-glib/json-glib.h>
#include <libvalent-core.h>

#include "valent-device.h"

G_BEGIN_DECLS

#define VALENT_TYPE_DEVICE_TRANSFER (valent_device_transfer_get_type())

VALENT_AVAILABLE_IN_1_0
G_DECLARE_FINAL_TYPE (ValentDeviceTransfer, valent_device_transfer, VALENT, DEVICE_TRANSFER, ValentTransfer)

VALENT_AVAILABLE_IN_1_0
ValentTransfer * valent_device_transfer_new_for_file (ValentDevice         *device,
                                                      JsonNode             *packet,
                                                      GFile                *file);
VALENT_AVAILABLE_IN_1_0
ValentDevice   * valent_device_transfer_ref_device   (ValentDeviceTransfer *transfer);
VALENT_AVAILABLE_IN_1_0
GFile          * valent_device_transfer_ref_file     (ValentDeviceTransfer *transfer);
VALENT_AVAILABLE_IN_1_0
JsonNode       * valent_device_transfer_ref_packet   (ValentDeviceTransfer *transfer);

G_END_DECLS
