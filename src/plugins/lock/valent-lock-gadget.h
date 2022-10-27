// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define VALENT_TYPE_LOCK_GADGET (valent_lock_gadget_get_type())

G_DECLARE_FINAL_TYPE (ValentLockGadget, valent_lock_gadget, VALENT, LOCK_GADGET, GtkWidget)

G_END_DECLS
