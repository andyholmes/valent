// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <valent.h>

G_BEGIN_DECLS

#define VALENT_TYPE_MPRIS_ADAPTER (valent_mpris_adapter_get_type ())

G_DECLARE_FINAL_TYPE (ValentMPRISAdapter, valent_mpris_adapter, VALENT, MPRIS_ADAPTER, ValentMediaAdapter)

G_END_DECLS

