// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_MEDIA_INSIDE) && !defined (VALENT_MEDIA_COMPILATION)
# error "Only <libvalent-media.h> can be included directly."
#endif

#include <glib.h>
#include <glib-object.h>

#include <libvalent-core.h>
#include "valent-media-player.h"

G_BEGIN_DECLS

#define VALENT_TYPE_MEDIA (valent_media_get_type ())

G_DECLARE_FINAL_TYPE (ValentMedia, valent_media, VALENT, MEDIA, ValentComponent)

GPtrArray         * valent_media_get_players        (ValentMedia *media);
ValentMediaPlayer * valent_media_get_player_by_name (ValentMedia *media,
                                                     const char  *name);

void                valent_media_pause              (ValentMedia *media);
void                valent_media_unpause            (ValentMedia *media);

ValentMedia       * valent_media_get_default        (void);

G_END_DECLS

