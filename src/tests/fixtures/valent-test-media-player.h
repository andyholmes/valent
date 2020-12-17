// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <libvalent-media.h>

G_BEGIN_DECLS

#define VALENT_TYPE_TEST_MEDIA_PLAYER (valent_test_media_player_get_type())

G_DECLARE_FINAL_TYPE (ValentTestMediaPlayer, valent_test_media_player, VALENT, TEST_MEDIA_PLAYER, ValentMediaPlayer)

G_END_DECLS

