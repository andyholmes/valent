// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_MIXER_INSIDE) && !defined (VALENT_MIXER_COMPILATION)
# error "Only <libvalent-mixer.h> can be included directly."
#endif

#include <libvalent-core.h>

#include "valent-mixer-stream.h"


G_BEGIN_DECLS

#define VALENT_TYPE_MIXER_ADAPTER (valent_mixer_adapter_get_type())

VALENT_AVAILABLE_IN_1_0
G_DECLARE_DERIVABLE_TYPE (ValentMixerAdapter, valent_mixer_adapter, VALENT, MIXER_ADAPTER, GObject)

struct _ValentMixerAdapterClass
{
  GObjectClass        parent_class;

  /* virtual functions */
  ValentMixerStream * (*get_default_input)  (ValentMixerAdapter *adapter);
  void                (*set_default_input)  (ValentMixerAdapter *adapter,
                                             ValentMixerStream  *stream);
  ValentMixerStream * (*get_default_output) (ValentMixerAdapter *adapter);
  void                (*set_default_output) (ValentMixerAdapter *adapter,
                                             ValentMixerStream  *stream);

  /* signals */
  void                (*stream_added)       (ValentMixerAdapter *adapter,
                                             ValentMixerStream  *stream);
  void                (*stream_removed)     (ValentMixerAdapter *adapter,
                                             ValentMixerStream  *stream);

  /*< private >*/
  gpointer            padding[8];
};

VALENT_AVAILABLE_IN_1_0
void                valent_mixer_adapter_stream_added       (ValentMixerAdapter *adapter,
                                                             ValentMixerStream  *stream);
VALENT_AVAILABLE_IN_1_0
void                valent_mixer_adapter_stream_removed     (ValentMixerAdapter *adapter,
                                                             ValentMixerStream  *stream);
VALENT_AVAILABLE_IN_1_0
ValentMixerStream * valent_mixer_adapter_get_default_input  (ValentMixerAdapter *adapter);
VALENT_AVAILABLE_IN_1_0
void                valent_mixer_adapter_set_default_input  (ValentMixerAdapter *adapter,
                                                             ValentMixerStream  *stream);
VALENT_AVAILABLE_IN_1_0
ValentMixerStream * valent_mixer_adapter_get_default_output (ValentMixerAdapter *adapter);
VALENT_AVAILABLE_IN_1_0
void                valent_mixer_adapter_set_default_output (ValentMixerAdapter *adapter,
                                                             ValentMixerStream  *stream);
VALENT_AVAILABLE_IN_1_0
GPtrArray         * valent_mixer_adapter_get_inputs         (ValentMixerAdapter *adapter);
VALENT_AVAILABLE_IN_1_0
GPtrArray         * valent_mixer_adapter_get_outputs         (ValentMixerAdapter *adapter);

G_END_DECLS

