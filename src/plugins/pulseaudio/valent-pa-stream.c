// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-pa-stream"

#include "config.h"

#include <math.h>

#include <gvc-mixer-stream.h>
#include <valent.h>

#include "valent-pa-stream.h"

#define _PA_VOLUME_NORM ((pa_volume_t) 0x10000U)

struct _ValentPaStream
{
  ValentMixerStream  parent_instance;

  GvcMixerStream    *stream;
  char              *description;
};

G_DEFINE_FINAL_TYPE (ValentPaStream, valent_pa_stream, VALENT_TYPE_MIXER_STREAM)

typedef enum {
  PROP_BASE_STREAM = 1,
} ValentPaStreamProperty;

static GParamSpec *properties[PROP_BASE_STREAM + 1] = { NULL, };


static void
on_port_changed (GvcMixerStream *stream,
                 GParamSpec     *pspec,
                 ValentPaStream *self)
{
  const GvcMixerStreamPort *port;

  g_assert (VALENT_IS_PA_STREAM (self));

  g_clear_pointer (&self->description, g_free);

  if ((port = gvc_mixer_stream_get_port (self->stream)) != NULL)
    {
      const char *description;

      description = gvc_mixer_stream_get_description (self->stream);
      self->description = g_strdup_printf ("%s (%s)",
                                           port->human_port,
                                           description);
    }

  g_object_notify (G_OBJECT (self), "description");
}

/*
 * ValentMixerStream
 */
static const char *
valent_pa_stream_get_description (ValentMixerStream *stream)
{
  ValentPaStream *self = VALENT_PA_STREAM (stream);

  g_assert (VALENT_IS_PA_STREAM (self));

  if (self->description == NULL)
    return gvc_mixer_stream_get_description (self->stream);

  return self->description;
}

static unsigned int
valent_pa_stream_get_level (ValentMixerStream *stream)
{
  ValentPaStream *self = VALENT_PA_STREAM (stream);
  pa_volume_t volume;
  double percent;

  g_assert (VALENT_IS_PA_STREAM (self));
  g_assert (GVC_IS_MIXER_STREAM (self->stream));

  volume = gvc_mixer_stream_get_volume (self->stream);
  percent = (double)volume / (double)_PA_VOLUME_NORM;

  return (unsigned int)round (percent * 100.0);
}

static void
valent_pa_stream_set_level (ValentMixerStream *stream,
                            unsigned int       level)
{
  ValentPaStream *self = VALENT_PA_STREAM (stream);
  double percent;
  pa_volume_t volume;

  g_assert (VALENT_IS_PA_STREAM (self));
  g_assert (GVC_IS_MIXER_STREAM (self->stream));

  percent = (double)level / 100.0;
  volume = (pa_volume_t)round (percent * _PA_VOLUME_NORM);

  gvc_mixer_stream_set_volume (self->stream, (pa_volume_t)volume);
  gvc_mixer_stream_push_volume (self->stream);
  g_object_notify (G_OBJECT (stream), "level");
}

static gboolean
valent_pa_stream_get_muted (ValentMixerStream *stream)
{
  ValentPaStream *self = VALENT_PA_STREAM (stream);

  g_assert (VALENT_IS_PA_STREAM (self));
  g_assert (GVC_IS_MIXER_STREAM (self->stream));

  return gvc_mixer_stream_get_is_muted (self->stream);
}

static void
valent_pa_stream_set_muted (ValentMixerStream *stream,
                            gboolean           state)
{
  ValentPaStream *self = VALENT_PA_STREAM (stream);

  g_assert (VALENT_IS_PA_STREAM (self));
  g_assert (GVC_IS_MIXER_STREAM (self->stream));

  gvc_mixer_stream_change_is_muted (self->stream, state);
  g_object_notify (G_OBJECT (stream), "muted");
}

static const char *
valent_pa_stream_get_name (ValentMixerStream *stream)
{
  ValentPaStream *self = VALENT_PA_STREAM (stream);

  g_assert (VALENT_IS_PA_STREAM (self));
  g_assert (GVC_IS_MIXER_STREAM (self->stream));

  return gvc_mixer_stream_get_name (self->stream);
}

/*
 * GObject
 */
static void
valent_pa_stream_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  ValentPaStream *self = VALENT_PA_STREAM (object);

  switch ((ValentPaStreamProperty)prop_id)
    {
    case PROP_BASE_STREAM:
      g_value_set_object (value, self->stream);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_pa_stream_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  ValentPaStream *self = VALENT_PA_STREAM (object);

  switch ((ValentPaStreamProperty)prop_id)
    {
    case PROP_BASE_STREAM:
      self->stream = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_pa_stream_constructed (GObject *object)
{
  ValentPaStream *self = VALENT_PA_STREAM (object);

  G_OBJECT_CLASS (valent_pa_stream_parent_class)->constructed (object);

  g_assert (self->stream != NULL);

  g_signal_connect_object (self->stream,
                           "notify::port",
                           G_CALLBACK (on_port_changed),
                           self, 0);
  on_port_changed (self->stream, NULL, self);
}

static void
valent_pa_stream_finalize (GObject *object)
{
  ValentPaStream *self = VALENT_PA_STREAM (object);

  g_signal_handlers_disconnect_by_data (self->stream, self);
  g_clear_object (&self->stream);

  G_OBJECT_CLASS (valent_pa_stream_parent_class)->finalize (object);
}

static void
valent_pa_stream_class_init (ValentPaStreamClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentMixerStreamClass *stream_class = VALENT_MIXER_STREAM_CLASS (klass);

  object_class->constructed = valent_pa_stream_constructed;
  object_class->finalize = valent_pa_stream_finalize;
  object_class->get_property = valent_pa_stream_get_property;
  object_class->set_property = valent_pa_stream_set_property;

  stream_class->get_name = valent_pa_stream_get_name;
  stream_class->get_description = valent_pa_stream_get_description;
  stream_class->get_level = valent_pa_stream_get_level;
  stream_class->set_level = valent_pa_stream_set_level;
  stream_class->get_muted = valent_pa_stream_get_muted;
  stream_class->set_muted = valent_pa_stream_set_muted;

  /**
   * ValentPaStream:base-stream:
   *
   * The `GvcMixerStream` this stream wraps.
   */
  properties [PROP_BASE_STREAM] =
    g_param_spec_object ("base-stream", NULL, NULL,
                         GVC_TYPE_MIXER_STREAM,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, G_N_ELEMENTS (properties), properties);
}

static void
valent_pa_stream_init (ValentPaStream *self)
{
}

