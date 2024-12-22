// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-pipewire-stream"

#include "config.h"

#include <math.h>

#include <pipewire/pipewire.h>
#include <valent.h>

#include "valent-pipewire-mixer.h"
#include "valent-pipewire-stream.h"


struct _ValentPipewireStream
{
  ValentMixerStream    parent_instance;

  ValentPipewireMixer *adapter;
  uint32_t             device_id;
  uint32_t             node_id;

  char                *description;
  unsigned int         level;
  gboolean             muted;
};

G_DEFINE_FINAL_TYPE (ValentPipewireStream, valent_pipewire_stream, VALENT_TYPE_MIXER_STREAM)

typedef enum {
  PROP_ADAPTER = 1,
  PROP_DEVICE_ID,
  PROP_NODE_ID,
} ValentPipewireStreamProperty;

static GParamSpec *properties[PROP_NODE_ID + 1] = { NULL, };


/*
 * ValentMixerStream
 */
static const char *
valent_pipewire_stream_get_description (ValentMixerStream *stream)
{
  ValentPipewireStream *self = VALENT_PIPEWIRE_STREAM (stream);

  g_assert (VALENT_IS_PIPEWIRE_STREAM (self));

  return self->description;
}

static unsigned int
valent_pipewire_stream_get_level (ValentMixerStream *stream)
{
  ValentPipewireStream *self = VALENT_PIPEWIRE_STREAM (stream);

  g_assert (VALENT_IS_PIPEWIRE_STREAM (self));

  return self->level;
}

static void
valent_pipewire_stream_set_level (ValentMixerStream *stream,
                                  unsigned int       level)
{
  ValentPipewireStream *self = VALENT_PIPEWIRE_STREAM (stream);

  g_assert (VALENT_IS_PIPEWIRE_STREAM (self));

  if (self->level == level || self->adapter == NULL)
    return;

  valent_pipewire_mixer_set_stream_state (self->adapter,
                                          self->device_id,
                                          self->node_id,
                                          level,
                                          self->muted);
}

static gboolean
valent_pipewire_stream_get_muted (ValentMixerStream *stream)
{
  ValentPipewireStream *self = VALENT_PIPEWIRE_STREAM (stream);

  g_assert (VALENT_IS_PIPEWIRE_STREAM (self));

  return self->muted;
}

static void
valent_pipewire_stream_set_muted (ValentMixerStream *stream,
                                  gboolean           state)
{
  ValentPipewireStream *self = VALENT_PIPEWIRE_STREAM (stream);

  g_assert (VALENT_IS_PIPEWIRE_STREAM (self));

  if (self->muted == state || self->adapter == NULL)
    return;

  valent_pipewire_mixer_set_stream_state (self->adapter,
                                          self->device_id,
                                          self->node_id,
                                          self->level,
                                          state);
}

/*
 * GObject
 */
static void
valent_pipewire_stream_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  ValentPipewireStream *self = VALENT_PIPEWIRE_STREAM (object);

  switch ((ValentPipewireStreamProperty)prop_id)
    {
    case PROP_ADAPTER:
      g_value_set_object (value, self->adapter);
      break;

    case PROP_DEVICE_ID:
      g_value_set_uint (value, self->device_id);
      break;

    case PROP_NODE_ID:
      g_value_set_uint (value, self->node_id);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_pipewire_stream_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  ValentPipewireStream *self = VALENT_PIPEWIRE_STREAM (object);

  switch ((ValentPipewireStreamProperty)prop_id)
    {
    case PROP_ADAPTER:
      self->adapter = g_value_get_object (value);
      g_object_add_weak_pointer (G_OBJECT (self->adapter),
                                 (gpointer *)&self->adapter);
      break;

    case PROP_DEVICE_ID:
      self->device_id = g_value_get_uint (value);
      break;

    case PROP_NODE_ID:
      self->node_id = g_value_get_uint (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_pipewire_stream_finalize (GObject *object)
{
  ValentPipewireStream *self = VALENT_PIPEWIRE_STREAM (object);

  g_clear_weak_pointer (&self->adapter);
  g_clear_pointer (&self->description, g_free);

  G_OBJECT_CLASS (valent_pipewire_stream_parent_class)->finalize (object);
}

static void
valent_pipewire_stream_class_init (ValentPipewireStreamClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentMixerStreamClass *stream_class = VALENT_MIXER_STREAM_CLASS (klass);

  object_class->finalize = valent_pipewire_stream_finalize;
  object_class->get_property = valent_pipewire_stream_get_property;
  object_class->set_property = valent_pipewire_stream_set_property;

  stream_class->get_description = valent_pipewire_stream_get_description;
  stream_class->get_level = valent_pipewire_stream_get_level;
  stream_class->set_level = valent_pipewire_stream_set_level;
  stream_class->get_muted = valent_pipewire_stream_get_muted;
  stream_class->set_muted = valent_pipewire_stream_set_muted;

  /**
   * ValentPaStream:adapter:
   *
   * The #GvcMixerStream this stream wraps.
   */
  properties [PROP_ADAPTER] =
    g_param_spec_object ("adapter", NULL, NULL,
                         VALENT_TYPE_PIPEWIRE_MIXER,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentPaStream:device-id:
   *
   * The PipeWire device ID.
   */
  properties [PROP_DEVICE_ID] =
    g_param_spec_uint ("device-id", NULL, NULL,
                       0, G_MAXUINT32,
                       0,
                       (G_PARAM_READWRITE |
                        G_PARAM_CONSTRUCT_ONLY |
                        G_PARAM_EXPLICIT_NOTIFY |
                        G_PARAM_STATIC_STRINGS));

  /**
   * ValentPaStream:node-id:
   *
   * The PipeWire node ID.
   */
  properties [PROP_NODE_ID] =
    g_param_spec_uint ("node-id", NULL, NULL,
                       0, G_MAXUINT32,
                       0,
                       (G_PARAM_READWRITE |
                        G_PARAM_CONSTRUCT_ONLY |
                        G_PARAM_EXPLICIT_NOTIFY |
                        G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, G_N_ELEMENTS (properties), properties);
}

static void
valent_pipewire_stream_init (ValentPipewireStream *self)
{
}

/*< private >
 * valent_pipewire_stream_update:
 * @stream: `ValentPipewireStream`
 * @description: the new description
 * @level: the new volume level
 * @state: the new mute state
 *
 * Update the stream state.
 */
void
valent_pipewire_stream_update (ValentPipewireStream *stream,
                               const char           *description,
                               uint32_t              level,
                               gboolean              state)
{
  g_return_if_fail (VALENT_IS_MIXER_STREAM (stream));

  if (g_set_str (&stream->description, description))
    g_object_notify (G_OBJECT (stream), "description");

  if (stream->level != level)
    {
      stream->level = level;
      g_object_notify (G_OBJECT (stream), "level");
    }

  if (stream->muted != state)
    {
      stream->muted = state;
      g_object_notify (G_OBJECT (stream), "muted");
    }
}

