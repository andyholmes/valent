// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <libportal/portal.h>

G_BEGIN_DECLS

XdpPortal * valent_xdp_get_default (void);
XdpParent * valent_xdp_get_parent  (void);
gboolean    valent_xdp_has_parent  (void);

G_END_DECLS
