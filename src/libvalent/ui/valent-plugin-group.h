// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define VALENT_TYPE_PLUGIN_GROUP (valent_plugin_group_get_type())

G_DECLARE_FINAL_TYPE (ValentPluginGroup, valent_plugin_group, VALENT, PLUGIN_GROUP, AdwPreferencesGroup)

G_END_DECLS
