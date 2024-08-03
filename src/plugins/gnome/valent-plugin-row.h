// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define VALENT_TYPE_PLUGIN_ROW (valent_plugin_row_get_type())

G_DECLARE_FINAL_TYPE (ValentPluginRow, valent_plugin_row, VALENT, PLUGIN_ROW, AdwExpanderRow)

G_END_DECLS
