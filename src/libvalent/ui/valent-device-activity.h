// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_UI_INSIDE) && !defined (VALENT_UI_COMPILATION)
# error "Only <libvalent-ui.h> can be included directly."
#endif

#include <gtk/gtk.h>
#include <libvalent-core.h>

G_BEGIN_DECLS

#define VALENT_TYPE_DEVICE_ACTIVITY (valent_device_activity_get_type ())

VALENT_AVAILABLE_IN_1_0
G_DECLARE_INTERFACE (ValentDeviceActivity, valent_device_activity, VALENT, DEVICE_ACTIVITY, GtkWidget)

struct _ValentDeviceActivityInterface
{
  GTypeInterface   g_iface;

  /*< private >*/
  gpointer         padding[8];
};

G_END_DECLS

