// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <libpeas/peas.h>

G_BEGIN_DECLS

#define VALENT_TYPE_XDP_INPUT (valent_xdp_input_get_type())

G_DECLARE_FINAL_TYPE (ValentXdpInput, valent_xdp_input, VALENT, XDP_INPUT, PeasExtensionBase)

G_END_DECLS

