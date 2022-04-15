// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_MIXER_INSIDE) && !defined (VALENT_MIXER_COMPILATION)
# error "Only <libvalent-mixer.h> can be included directly."
#endif

#include <libvalent-core.h>

#include "valent-mixer-stream.h"

G_BEGIN_DECLS

#define VALENT_TYPE_MIXER (valent_mixer_get_type ())

VALENT_AVAILABLE_IN_1_0
G_DECLARE_FINAL_TYPE (ValentMixer, valent_mixer, VALENT, MIXER, ValentComponent)

VALENT_AVAILABLE_IN_1_0
ValentMixer       * valent_mixer_get_default        (void);
VALENT_AVAILABLE_IN_1_0
ValentMixerStream * valent_mixer_get_default_input  (ValentMixer       *mixer);
VALENT_AVAILABLE_IN_1_0
void                valent_mixer_set_default_input  (ValentMixer       *mixer,
                                                     ValentMixerStream *stream);
VALENT_AVAILABLE_IN_1_0
ValentMixerStream * valent_mixer_get_default_output (ValentMixer       *mixer);
VALENT_AVAILABLE_IN_1_0
void                valent_mixer_set_default_output (ValentMixer       *mixer,
                                                     ValentMixerStream *stream);
VALENT_AVAILABLE_IN_1_0
GPtrArray         * valent_mixer_get_inputs         (ValentMixer       *mixer);
VALENT_AVAILABLE_IN_1_0
GPtrArray         * valent_mixer_get_outputs        (ValentMixer       *mixer);

G_END_DECLS

