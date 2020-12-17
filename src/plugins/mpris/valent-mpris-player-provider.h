// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <libvalent-media.h>

G_BEGIN_DECLS

#define VALENT_TYPE_MPRIS_PLAYER_PROVIDER (valent_mpris_player_provider_get_type ())

G_DECLARE_FINAL_TYPE (ValentMPRISPlayerProvider, valent_mpris_player_provider, VALENT, MPRIS_PLAYER_PROVIDER, ValentMediaPlayerProvider)

G_END_DECLS

