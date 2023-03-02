// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <valent.h>

G_BEGIN_DECLS

#define VALENT_TYPE_NOTIFICATION_PLUGIN (valent_notification_plugin_get_type())

G_DECLARE_FINAL_TYPE (ValentNotificationPlugin, valent_notification_plugin, VALENT, NOTIFICATION_PLUGIN, ValentDevicePlugin)

G_END_DECLS

