// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <libvalent-media.h>

G_BEGIN_DECLS

#define VALENT_TYPE_MPRIS_DEVICE (valent_mpris_device_get_type())

G_DECLARE_FINAL_TYPE (ValentMprisDevice, valent_mpris_device, VALENT, MPRIS_DEVICE, ValentMediaPlayer)

ValentMprisDevice * valent_mpris_device_new             (ValentDevice         *player);
void                valent_mpris_device_handle_packet   (ValentMprisDevice    *player,
                                                         JsonNode             *packet);
void                valent_mpris_device_update_art      (ValentMprisDevice    *player,
                                                         GFile                *file);
void                valent_mpris_device_update_flags    (ValentMprisDevice    *player,
                                                         ValentMediaActions    flags);
void                valent_mpris_device_update_metadata (ValentMprisDevice    *player,
                                                         GVariant             *metadata);
void                valent_mpris_device_update_name     (ValentMprisDevice    *player,
                                                         const char           *name);
void                valent_mpris_device_update_position (ValentMprisDevice    *player,
                                                         gint64                position);
void                valent_mpris_device_update_repeat   (ValentMprisDevice    *player,
                                                         const char           *loop_status);
void                valent_mpris_device_update_shuffle  (ValentMprisDevice    *player,
                                                         gboolean              shuffle);
void                valent_mpris_device_update_state    (ValentMprisDevice    *player,
                                                         const char           *playback_status);
void                valent_mpris_device_update_volume   (ValentMprisDevice    *player,
                                                         double                volume);

G_END_DECLS

