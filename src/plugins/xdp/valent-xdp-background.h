// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <valent.h>

G_BEGIN_DECLS

#define VALENT_TYPE_XDP_BACKGROUND (valent_xdp_background_get_type())

G_DECLARE_FINAL_TYPE (ValentXdpBackground, valent_xdp_background, VALENT, XDP_BACKGROUND, ValentApplicationPlugin)

G_END_DECLS

