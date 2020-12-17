// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <libvalent-media.h>

G_BEGIN_DECLS

#define VALENT_TYPE_MPRIS_PLAYER (valent_mpris_player_get_type())

G_DECLARE_FINAL_TYPE (ValentMPRISPlayer, valent_mpris_player, VALENT, MPRIS_PLAYER, ValentMediaPlayer)

void                valent_mpris_player_new              (const char           *name,
                                                          GCancellable         *cancellable,
                                                          GAsyncReadyCallback   callback,
                                                          gpointer              user_data);
ValentMPRISPlayer * valent_mpris_player_new_finish       (GAsyncResult         *result,
                                                          GError              **error);

G_END_DECLS

