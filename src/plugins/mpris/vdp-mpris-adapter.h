// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <valent.h>

G_BEGIN_DECLS

#define VDP_TYPE_MPRIS_ADAPTER (vdp_mpris_adapter_get_type())

G_DECLARE_FINAL_TYPE (VdpMprisAdapter, vdp_mpris_adapter, VDP, MPRIS_ADAPTER, ValentMediaAdapter)

ValentMediaAdapter * vdp_mpris_adapter_new           (ValentDevice    *device);
void                 vdp_mpris_adapter_handle_packet (VdpMprisAdapter *self,
                                                      JsonNode        *packet);

G_END_DECLS

