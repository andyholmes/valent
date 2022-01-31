// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_MIXER_INSIDE) && !defined (VALENT_MIXER_COMPILATION)
# error "Only <libvalent-mixer.h> can be included directly."
#endif

#include <libvalent-core.h>

#include "valent-mixer-stream.h"


G_BEGIN_DECLS

#define VALENT_TYPE_MIXER_CONTROL (valent_mixer_control_get_type())

VALENT_AVAILABLE_IN_1_0
G_DECLARE_DERIVABLE_TYPE (ValentMixerControl, valent_mixer_control, VALENT, MIXER_CONTROL, GObject)

struct _ValentMixerControlClass
{
  GObjectClass        parent_class;

  /* virtual functions */
  ValentMixerStream * (*get_default_input)  (ValentMixerControl *control);
  ValentMixerStream * (*get_default_output) (ValentMixerControl *control);

  /* signals */
  void                (*stream_added)       (ValentMixerControl *control,
                                             ValentMixerStream  *stream);
  void                (*stream_changed)     (ValentMixerControl *control,
                                             ValentMixerStream  *stream);
  void                (*stream_removed)     (ValentMixerControl *control,
                                             ValentMixerStream  *stream);
};

/* Signal Quarks */
VALENT_AVAILABLE_IN_1_0
GQuark              valent_mixer_stream_input_quark          (void);
VALENT_AVAILABLE_IN_1_0
GQuark              valent_mixer_stream_output_quark         (void);

/* Core Interface */
VALENT_AVAILABLE_IN_1_0
void                valent_mixer_control_emit_stream_added   (ValentMixerControl *control,
                                                              ValentMixerStream  *stream);
VALENT_AVAILABLE_IN_1_0
void                valent_mixer_control_emit_stream_changed (ValentMixerControl *control,
                                                              ValentMixerStream  *stream);
VALENT_AVAILABLE_IN_1_0
void                valent_mixer_control_emit_stream_removed (ValentMixerControl *control,
                                                              ValentMixerStream  *stream);
VALENT_AVAILABLE_IN_1_0
ValentMixerStream * valent_mixer_control_get_default_input   (ValentMixerControl *control);
VALENT_AVAILABLE_IN_1_0
ValentMixerStream * valent_mixer_control_get_default_output  (ValentMixerControl *control);
VALENT_AVAILABLE_IN_1_0
GPtrArray         * valent_mixer_control_get_inputs          (ValentMixerControl *control);
VALENT_AVAILABLE_IN_1_0
GPtrArray         * valent_mixer_control_get_outputs         (ValentMixerControl *control);

G_END_DECLS

