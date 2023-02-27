// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <valent.h>

G_BEGIN_DECLS

#define VALENT_TYPE_PRESENTER_PLUGIN (valent_presenter_plugin_get_type())

G_DECLARE_FINAL_TYPE (ValentPresenterPlugin, valent_presenter_plugin, VALENT, PRESENTER_PLUGIN, ValentDevicePlugin)

G_END_DECLS

