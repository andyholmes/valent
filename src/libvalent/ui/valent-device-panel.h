// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <gtk/gtk.h>
#include <libvalent-core.h>

G_BEGIN_DECLS

#define VALENT_TYPE_DEVICE_PANEL (valent_device_panel_get_type())

G_DECLARE_FINAL_TYPE (ValentDevicePanel, valent_device_panel, VALENT, DEVICE_PANEL, GtkWidget)

GtkWidget    * valent_device_panel_new        (ValentDevice      *device);
ValentDevice * valent_device_panel_get_device (ValentDevicePanel *panel);

G_END_DECLS
