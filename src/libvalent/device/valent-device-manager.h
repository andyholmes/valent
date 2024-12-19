// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_INSIDE) && !defined (VALENT_COMPILATION)
# error "Only <valent.h> can be included directly."
#endif

#include "../core/valent-application-plugin.h"

G_BEGIN_DECLS

#define VALENT_TYPE_DEVICE_MANAGER (valent_device_manager_get_type())

VALENT_AVAILABLE_IN_1_0
G_DECLARE_FINAL_TYPE (ValentDeviceManager, valent_device_manager, VALENT, DEVICE_MANAGER, ValentApplicationPlugin)

VALENT_AVAILABLE_IN_1_0
ValentDeviceManager * valent_device_manager_get_default (void);
VALENT_AVAILABLE_IN_1_0
void                  valent_device_manager_refresh     (ValentDeviceManager *manager);

G_END_DECLS
