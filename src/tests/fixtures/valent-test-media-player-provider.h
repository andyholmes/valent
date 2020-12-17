// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <libvalent-media.h>

G_BEGIN_DECLS

#define VALENT_TYPE_TEST_MEDIA_PLAYER_PROVIDER (valent_test_media_player_provider_get_type ())

G_DECLARE_FINAL_TYPE (ValentTestMediaPlayerProvider, valent_test_media_player_provider, VALENT, TEST_MEDIA_PLAYER_PROVIDER, ValentMediaPlayerProvider)

G_END_DECLS

