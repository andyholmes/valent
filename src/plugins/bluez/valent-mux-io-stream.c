// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-mux-io-stream"

#include "config.h"

#include <gio/gio.h>

#include "valent-mux-connection.h"
#include "valent-mux-io-stream.h"
#include "valent-mux-input-stream.h"
#include "valent-mux-output-stream.h"


struct _ValentMuxIOStream
{
  GIOStream            parent_instance;

  ValentMuxConnection *muxer;
  char                *uuid;

  GInputStream        *input_stream;
  GOutputStream       *output_stream;
};

G_DEFINE_FINAL_TYPE (ValentMuxIOStream, valent_mux_io_stream, G_TYPE_IO_STREAM)

enum {
  PROP_0,
  PROP_MUXER,
  PROP_UUID,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };


/*
 * GIOStream
 */
static GInputStream *
valent_mux_io_stream_get_input_stream (GIOStream *stream)
{
  ValentMuxIOStream *self = VALENT_MUX_IO_STREAM (stream);

  return self->input_stream;
}

static GOutputStream *
valent_mux_io_stream_get_output_stream (GIOStream *stream)
{
  ValentMuxIOStream *self = VALENT_MUX_IO_STREAM (stream);

  return self->output_stream;
}

static gboolean
valent_mux_io_stream_close_fn (GIOStream     *stream,
                               GCancellable  *cancellable,
                               GError       **error)
{
  ValentMuxIOStream *self = VALENT_MUX_IO_STREAM (stream);
  gboolean ret = TRUE;

  if (self->output_stream != NULL)
    ret = g_output_stream_close (self->output_stream, cancellable, error);

  if (error != NULL && *error != NULL)
    error = NULL;

  if (self->input_stream != NULL)
    ret &= g_input_stream_close (self->input_stream, cancellable, error);

  if (error != NULL && *error != NULL)
    error = NULL;

  if (self->muxer != NULL && self->uuid != NULL)
    {
      ret = valent_mux_connection_close_channel (self->muxer,
                                                 self->uuid,
                                                 cancellable,
                                                 error);
    }

  return ret;
}

/*
 * GObject
 */
static void
valent_mux_io_stream_constructed (GObject *object)
{
  ValentMuxIOStream *self = VALENT_MUX_IO_STREAM (object);

  if (self->muxer != NULL && self->uuid != NULL)
    {
      self->input_stream = g_object_new (VALENT_TYPE_MUX_INPUT_STREAM,
                                         "muxer", self->muxer,
                                         "uuid",  self->uuid,
                                         NULL);
      self->output_stream = g_object_new (VALENT_TYPE_MUX_OUTPUT_STREAM,
                                          "muxer", self->muxer,
                                          "uuid",  self->uuid,
                                          NULL);
    }

  G_OBJECT_CLASS (valent_mux_io_stream_parent_class)->constructed (object);
}

static void
valent_mux_io_stream_finalize (GObject *object)
{
  ValentMuxIOStream *self = VALENT_MUX_IO_STREAM (object);

  g_clear_object (&self->input_stream);
  g_clear_object (&self->output_stream);
  g_clear_object (&self->muxer);
  g_clear_pointer (&self->uuid, g_free);

  G_OBJECT_CLASS (valent_mux_io_stream_parent_class)->finalize (object);
}

static void
valent_mux_io_stream_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  ValentMuxIOStream *self = VALENT_MUX_IO_STREAM (object);

  switch (prop_id)
    {
    case PROP_MUXER:
      g_value_set_object (value, self->muxer);
      break;

    case PROP_UUID:
      g_value_set_string (value, self->uuid);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_mux_io_stream_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  ValentMuxIOStream *self = VALENT_MUX_IO_STREAM (object);

  switch (prop_id)
    {
    case PROP_MUXER:
      self->muxer = g_value_dup_object (value);
      break;

    case PROP_UUID:
      self->uuid = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_mux_io_stream_class_init (ValentMuxIOStreamClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GIOStreamClass *stream_class = G_IO_STREAM_CLASS (klass);

  object_class->constructed = valent_mux_io_stream_constructed;
  object_class->finalize = valent_mux_io_stream_finalize;
  object_class->get_property = valent_mux_io_stream_get_property;
  object_class->set_property = valent_mux_io_stream_set_property;

  stream_class->close_fn = valent_mux_io_stream_close_fn;
  stream_class->get_input_stream = valent_mux_io_stream_get_input_stream;
  stream_class->get_output_stream = valent_mux_io_stream_get_output_stream;

  /**
   * ValentMuxIOStream:muxer:
   *
   * Multiplexer that muxes and demuxes data for this stream.
   */
  properties [PROP_MUXER] =
    g_param_spec_object ("muxer",
                         "Muxer",
                         "Multiplexer that muxes and demuxes data for this stream",
                         VALENT_TYPE_MUX_CONNECTION,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentMuxIOStream:uuid:
   *
   * UUID of the channel this stream represents.
   */
  properties [PROP_UUID] =
    g_param_spec_string ("uuid",
                         "UUID",
                         "UUID of the channel this stream represents",
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
valent_mux_io_stream_init (ValentMuxIOStream *self)
{
}

/**
 * valent_mux_io_stream_new:
 * @muxer: a `ValentMuxConnection`
 * @uuid: a UUID
 *
 * Creates a new `ValentMuxIOStream` for @uuid, muxed by @muxer.
 *
 * Returns: (transfer full): a new `ValentMuxIOStream`
 */
GIOStream *
valent_mux_io_stream_new (ValentMuxConnection *muxer,
                          const char          *uuid)
{
  g_return_val_if_fail (VALENT_IS_MUX_CONNECTION (muxer), NULL);
  g_return_val_if_fail (uuid != NULL && *uuid != '\0', NULL);

  return g_object_new (G_TYPE_SIMPLE_IO_STREAM,
                       "muxer",        muxer,
                       "uuid",         uuid,
                       NULL);
}

/**
 * valent_mux_io_stream_get_uuid:
 * @stream: a `ValentMuxIOStream`
 *
 * Get the UUID for @stream.
 *
 * Returns: (transfer none): a UUID
 */
const char *
valent_mux_io_stream_get_uuid (ValentMuxIOStream *stream)
{
  g_return_val_if_fail (VALENT_IS_MUX_IO_STREAM (stream), NULL);

  return stream->uuid;
}

