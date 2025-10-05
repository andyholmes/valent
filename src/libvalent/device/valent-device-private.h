// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include "valent-device.h"

G_BEGIN_DECLS

_VALENT_EXTERN
ValentDevice * valent_device_new_full      (JsonNode      *identity,
                                            ValentContext *context);

G_END_DECLS
