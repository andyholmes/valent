// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_MIXER_INSIDE) && !defined (VALENT_MIXER_COMPILATION)
# error "Only <libvalent-mixer.h> can be included directly."
#endif

#include <libvalent-core.h>


G_BEGIN_DECLS

/**
 * ValentMixerDirection:
 * @VALENT_MIXER_INPUT: An input stream or "source" (eg. microphone)
 * @VALENT_MIXER_OUTPUT: An output stream or "sink" (eg. speakers)
 *
 * Enumeration of stream directions.
 */
typedef enum
{
  VALENT_MIXER_INPUT,
  VALENT_MIXER_OUTPUT,
} ValentMixerDirection;


#define VALENT_TYPE_MIXER_STREAM (valent_mixer_stream_get_type())

VALENT_AVAILABLE_IN_1_0
G_DECLARE_DERIVABLE_TYPE (ValentMixerStream, valent_mixer_stream, VALENT, MIXER_STREAM, GObject)

struct _ValentMixerStreamClass
{
  GObjectClass           parent_class;

  const char           * (*get_name)        (ValentMixerStream *stream);
  const char           * (*get_description) (ValentMixerStream *stream);
  ValentMixerDirection   (*get_direction)   (ValentMixerStream *stream);
  unsigned int           (*get_level)       (ValentMixerStream *stream);
  void                   (*set_level)       (ValentMixerStream *stream,
                                             unsigned int       level);
  gboolean               (*get_muted)       (ValentMixerStream *stream);
  void                   (*set_muted)       (ValentMixerStream *stream,
                                             gboolean           state);
};

VALENT_AVAILABLE_IN_1_0
const char           * valent_mixer_stream_get_name        (ValentMixerStream *stream);
VALENT_AVAILABLE_IN_1_0
const char           * valent_mixer_stream_get_description (ValentMixerStream *stream);
VALENT_AVAILABLE_IN_1_0
ValentMixerDirection   valent_mixer_stream_get_direction   (ValentMixerStream *stream);
VALENT_AVAILABLE_IN_1_0
unsigned int           valent_mixer_stream_get_level       (ValentMixerStream *stream);
VALENT_AVAILABLE_IN_1_0
void                   valent_mixer_stream_set_level       (ValentMixerStream *stream,
                                                            unsigned int       level);
VALENT_AVAILABLE_IN_1_0
gboolean               valent_mixer_stream_get_muted       (ValentMixerStream *stream);
VALENT_AVAILABLE_IN_1_0
void                   valent_mixer_stream_set_muted       (ValentMixerStream *stream,
                                                            gboolean           state);

G_END_DECLS

