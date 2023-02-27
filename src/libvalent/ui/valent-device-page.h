// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <gtk/gtk.h>

#include "../core/valent-object.h"

G_BEGIN_DECLS

#define VALENT_TYPE_DEVICE_PAGE (valent_device_page_get_type())

_VALENT_EXTERN
G_DECLARE_FINAL_TYPE (ValentDevicePage, valent_device_page, VALENT, DEVICE_PAGE, GtkBox)

void   valent_device_page_close_preferences (ValentDevicePage *panel);

G_END_DECLS
