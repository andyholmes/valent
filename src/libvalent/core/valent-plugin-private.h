// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <libpeas.h>

G_BEGIN_DECLS

char            * valent_plugin_get_settings_path   (PeasPluginInfo *plugin_info,
                                                     const char     *plugin_domain);
GSettingsSchema * valent_plugin_get_settings_schema (PeasPluginInfo *plugin_info,
                                                     const char     *schema_id);

G_END_DECLS

