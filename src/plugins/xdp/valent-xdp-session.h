// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <valent.h>

G_BEGIN_DECLS

#define VALENT_TYPE_XDP_SESSION (valent_xdp_session_get_type())

G_DECLARE_FINAL_TYPE (ValentXdpSession, valent_xdp_session, VALENT, XDP_SESSION, ValentSessionAdapter)

G_END_DECLS

