// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include <libpeas/peas.h>

G_BEGIN_DECLS

#define VALENT_TYPE_PLUGIN_ROW (valent_plugin_row_get_type())

G_DECLARE_FINAL_TYPE (ValentPluginRow, valent_plugin_row, VALENT, PLUGIN_ROW, AdwActionRow)

G_END_DECLS
