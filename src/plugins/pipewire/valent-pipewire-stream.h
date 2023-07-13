// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <valent.h>

G_BEGIN_DECLS

#define VALENT_TYPE_PIPEWIRE_STREAM (valent_pipewire_stream_get_type ())

G_DECLARE_FINAL_TYPE (ValentPipewireStream, valent_pipewire_stream, VALENT, PIPEWIRE_STREAM, ValentMixerStream)

void   valent_pipewire_stream_update (ValentPipewireStream *stream,
                                      const char           *description,
                                      uint32_t              level,
                                      gboolean              state);

G_END_DECLS

