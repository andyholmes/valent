// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <libvalent-media.h>

G_BEGIN_DECLS

#define VALENT_TYPE_MOCK_MEDIA_PLAYER (valent_mock_media_player_get_type())

G_DECLARE_FINAL_TYPE (ValentMockMediaPlayer, valent_mock_media_player, VALENT, MOCK_MEDIA_PLAYER, ValentMediaPlayer)

G_END_DECLS

