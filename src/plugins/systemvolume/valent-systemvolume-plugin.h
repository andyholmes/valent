// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <valent.h>

G_BEGIN_DECLS

#define VALENT_TYPE_SYSTEMVOLUME_PLUGIN (valent_systemvolume_plugin_get_type())

G_DECLARE_FINAL_TYPE (ValentSystemvolumePlugin, valent_systemvolume_plugin, VALENT, SYSTEMVOLUME_PLUGIN, ValentDevicePlugin)

G_END_DECLS

