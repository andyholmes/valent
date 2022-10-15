// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <libvalent-media.h>

G_BEGIN_DECLS

#define VALENT_TYPE_MPRIS_PLAYER (valent_mpris_player_get_type())

G_DECLARE_FINAL_TYPE (ValentMPRISPlayer, valent_mpris_player, VALENT, MPRIS_PLAYER, ValentMediaPlayer)

G_END_DECLS

