// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <gio/gio.h>

G_BEGIN_DECLS

#include "valent-device-enums.h"

#define VALENT_DEVICE_INSIDE

#include "valent-channel.h"
#include "valent-channel-service.h"
#include "valent-device.h"
#include "valent-device-manager.h"
#include "valent-device-plugin.h"
#include "valent-device-transfer.h"
#include "valent-packet.h"

#undef VALENT_DEVICE_INSIDE

G_END_DECLS

