// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <valent.h>

G_BEGIN_DECLS

#define VALENT_TYPE_MPRIS_PLAYER (vdp_mpris_player_get_type())

G_DECLARE_FINAL_TYPE (VdpMprisPlayer, vdp_mpris_player, VDP, MPRIS_PLAYER, ValentMediaPlayer)

VdpMprisPlayer * vdp_mpris_player_new           (ValentDevice   *player);
void             vdp_mpris_player_handle_packet (VdpMprisPlayer *player,
                                                 JsonNode       *packet);
void             vdp_mpris_player_update_art    (VdpMprisPlayer *player,
                                                 GFile          *file);
void             vdp_mpris_player_update_name   (VdpMprisPlayer *player,
                                                 const char     *name);

G_END_DECLS

