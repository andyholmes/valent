// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_TEST_INSIDE) && !defined (VALENT_TEST_COMPILATION)
# error "Only <libvalent-test.h> can be included directly."
#endif

#include <libvalent-media.h>

G_BEGIN_DECLS

#define VALENT_TYPE_MOCK_MEDIA_PLAYER_PROVIDER (valent_mock_media_player_provider_get_type ())

G_DECLARE_FINAL_TYPE (ValentMockMediaPlayerProvider, valent_mock_media_player_provider, VALENT, MOCK_MEDIA_PLAYER_PROVIDER, ValentMediaPlayerProvider)

ValentMediaPlayerProvider * valent_mock_media_player_provider_get_instance (void);

G_END_DECLS

