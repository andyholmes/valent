// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_UI_INSIDE) && !defined (VALENT_UI_COMPILATION)
# error "Only <libvalent-ui.h> can be included directly."
#endif

#include <glib-object.h>

G_BEGIN_DECLS

#define VALENT_TYPE_DEVICE_GADGET (valent_device_gadget_get_type ())

G_DECLARE_INTERFACE (ValentDeviceGadget, valent_device_gadget, VALENT, DEVICE_GADGET, GObject)

struct _ValentDeviceGadgetInterface
{
  GTypeInterface   g_iface;
};

G_END_DECLS

