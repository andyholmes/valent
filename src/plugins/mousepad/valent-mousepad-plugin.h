// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <libpeas/peas.h>

G_BEGIN_DECLS

#define VALENT_TYPE_MOUSEPAD_PLUGIN (valent_mousepad_plugin_get_type())

G_DECLARE_FINAL_TYPE (ValentMousepadPlugin, valent_mousepad_plugin, VALENT, MOUSEPAD_PLUGIN, PeasExtensionBase)

G_END_DECLS

