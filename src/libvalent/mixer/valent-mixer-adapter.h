// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_INSIDE) && !defined (VALENT_COMPILATION)
# error "Only <valent.h> can be included directly."
#endif

#include "valent-mixer-stream.h"

G_BEGIN_DECLS

#define VALENT_TYPE_MIXER_ADAPTER (valent_mixer_adapter_get_type())

VALENT_AVAILABLE_IN_1_0
G_DECLARE_DERIVABLE_TYPE (ValentMixerAdapter, valent_mixer_adapter, VALENT, MIXER_ADAPTER, ValentObject)

struct _ValentMixerAdapterClass
{
  ValentObjectClass   parent_class;

  /* virtual functions */
  ValentMixerStream * (*get_default_input)  (ValentMixerAdapter *adapter);
  void                (*set_default_input)  (ValentMixerAdapter *adapter,
                                             ValentMixerStream  *stream);
  ValentMixerStream * (*get_default_output) (ValentMixerAdapter *adapter);
  void                (*set_default_output) (ValentMixerAdapter *adapter,
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

G_END_DECLS

