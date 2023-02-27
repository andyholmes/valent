// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <valent.h>

G_BEGIN_DECLS

#define VALENT_TYPE_LOCK_GADGET (valent_lock_gadget_get_type())

G_DECLARE_FINAL_TYPE (ValentLockGadget, valent_lock_gadget, VALENT, LOCK_GADGET, ValentDeviceGadget)

G_END_DECLS
