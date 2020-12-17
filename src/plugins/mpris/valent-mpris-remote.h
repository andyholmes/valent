// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <libvalent-media.h>

G_BEGIN_DECLS

#define VALENT_TYPE_MPRIS_REMOTE (valent_mpris_remote_get_type())

G_DECLARE_FINAL_TYPE (ValentMprisRemote, valent_mpris_remote, VALENT, MPRIS_REMOTE, ValentMediaPlayer)

ValentMprisRemote * valent_mpris_remote_new                    (void);
void                valent_mpris_remote_export                 (ValentMprisRemote    *remote,
                                                                GCancellable         *cancellable,
                                                                GAsyncReadyCallback   callback,
                                                                gpointer              user_data);
gboolean            valent_mpris_remote_export_finish          (ValentMprisRemote    *remote,
                                                                GAsyncResult         *result,
                                                                GError              **error);
void                valent_mpris_remote_unexport               (ValentMprisRemote    *remote);

void                valent_mpris_remote_set_bus_name           (ValentMprisRemote    *remote,
                                                                const char           *name);
void                valent_mpris_remote_set_name               (ValentMprisRemote    *remote,
                                                                const char           *identity);
void                valent_mpris_remote_emit_seeked            (ValentMprisRemote    *remote,
                                                                gint64                position);
void                valent_mpris_remote_update_player          (ValentMprisRemote    *remote,
                                                                ValentMediaActions    flags,
                                                                GVariant             *metadata,
                                                                const char           *playback_status,
                                                                gint64                position,
                                                                double                volume);

void                valent_mpris_remote_update_art             (ValentMprisRemote    *remote,
                                                                GFile                *file);
void                valent_mpris_remote_update_metadata        (ValentMprisRemote    *remote,
                                                                GVariant             *metadata);
void                valent_mpris_remote_update_playback_status (ValentMprisRemote    *remote,
                                                                const char           *status);
void                valent_mpris_remote_update_position        (ValentMprisRemote    *remote,
                                                                gint64                position);
void                valent_mpris_remote_update_volume          (ValentMprisRemote    *remote,
                                                                double                volume);

G_END_DECLS

