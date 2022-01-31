// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_MIXER_INSIDE) && !defined (VALENT_MIXER_COMPILATION)
# error "Only <libvalent-mixer.h> can be included directly."
#endif

#include <libvalent-core.h>


G_BEGIN_DECLS

/**
 * ValentMixerStreamFlags:
 * @VALENT_MIXER_STREAM_UNKNOWN: no flags
 * @VALENT_MIXER_STREAM_SINK: A mixer output stream (eg. speakers)
 * @VALENT_MIXER_STREAM_SOURCE: A mixer input stream (eg. microphone)
 * @VALENT_MIXER_STREAM_LOCAL: A local stream
 * @VALENT_MIXER_STREAM_REMOTE: A remote stream
 * @VALENT_MIXER_STREAM_RESERVED1: Reserved
 * @VALENT_MIXER_STREAM_RESERVED2: Reserved
 * @VALENT_MIXER_STREAM_RESERVED3: Reserved
 * @VALENT_MIXER_STREAM_RESERVED4: Reserved
 *
 * Flags describing a #ValentMixerStream.
 */
typedef enum
{
  VALENT_MIXER_STREAM_UNKNOWN,
  VALENT_MIXER_STREAM_SINK      = (1<<0),
  VALENT_MIXER_STREAM_SOURCE    = (1<<1),
  VALENT_MIXER_STREAM_LOCAL     = (1<<2),
  VALENT_MIXER_STREAM_REMOTE    = (1<<3),
  VALENT_MIXER_STREAM_RESERVED1 = (1<<4),
  VALENT_MIXER_STREAM_RESERVED2 = (1<<5),
  VALENT_MIXER_STREAM_RESERVED3 = (1<<6),
  VALENT_MIXER_STREAM_RESERVED4 = (1<<7),
} ValentMixerStreamFlags;


#define VALENT_TYPE_MIXER_STREAM (valent_mixer_stream_get_type())

VALENT_AVAILABLE_IN_1_0
G_DECLARE_DERIVABLE_TYPE (ValentMixerStream, valent_mixer_stream, VALENT, MIXER_STREAM, GObject)

struct _ValentMixerStreamClass
{
  GObjectClass   parent_class;

  const char   * (*get_name)        (ValentMixerStream *stream);
  const char   * (*get_description) (ValentMixerStream *stream);

  unsigned int   (*get_level)       (ValentMixerStream *stream);
  void           (*set_level)       (ValentMixerStream *stream,
                                     unsigned int       level);
  gboolean       (*get_muted)       (ValentMixerStream *stream);
  void           (*set_muted)       (ValentMixerStream *stream,
                                     gboolean           state);
};

VALENT_AVAILABLE_IN_1_0
const char             * valent_mixer_stream_get_name        (ValentMixerStream *stream);
VALENT_AVAILABLE_IN_1_0
const char             * valent_mixer_stream_get_description (ValentMixerStream *stream);
VALENT_AVAILABLE_IN_1_0
ValentMixerStreamFlags   valent_mixer_stream_get_flags       (ValentMixerStream *stream);
VALENT_AVAILABLE_IN_1_0
unsigned int             valent_mixer_stream_get_level       (ValentMixerStream *stream);
VALENT_AVAILABLE_IN_1_0
void                     valent_mixer_stream_set_level       (ValentMixerStream *stream,
                                                              unsigned int       level);
VALENT_AVAILABLE_IN_1_0
gboolean                 valent_mixer_stream_get_muted       (ValentMixerStream *stream);
VALENT_AVAILABLE_IN_1_0
void                     valent_mixer_stream_set_muted       (ValentMixerStream *stream,
                                                              gboolean           state);

G_END_DECLS

