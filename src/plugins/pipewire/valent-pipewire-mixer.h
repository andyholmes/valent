// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <valent.h>

G_BEGIN_DECLS

#define VALENT_TYPE_PIPEWIRE_MIXER (valent_pipewire_mixer_get_type ())

G_DECLARE_FINAL_TYPE (ValentPipewireMixer, valent_pipewire_mixer, VALENT, PIPEWIRE_MIXER, ValentMixerAdapter)

void   valent_pipewire_mixer_set_stream_state (ValentPipewireMixer *adapter,
                                               uint32_t             device_id,
                                               uint32_t             node_id,
                                               unsigned int         level,
                                               gboolean             muted);

G_END_DECLS

