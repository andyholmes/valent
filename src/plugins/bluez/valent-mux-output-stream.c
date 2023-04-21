// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-mux-output-stream"

#include "config.h"

#include <gio/gio.h>

#include "valent-mux-connection.h"
#include "valent-mux-output-stream.h"


struct _ValentMuxOutputStream
{
  GOutputStream        parent_instance;

  ValentMuxConnection *muxer;
  char                *uuid;
};

G_DEFINE_FINAL_TYPE (ValentMuxOutputStream, valent_mux_output_stream, G_TYPE_OUTPUT_STREAM)

enum {
  PROP_0,
  PROP_MUXER,
  PROP_UUID,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };


/*
 * GOutputStream
 */
static gssize
valent_mux_output_stream_write (GOutputStream  *stream,
                                const void     *buffer,
                                size_t          count,
                                GCancellable   *cancellable,
                                GError        **error)
{
  ValentMuxOutputStream *self = VALENT_MUX_OUTPUT_STREAM (stream);

  g_assert (VALENT_IS_MUX_OUTPUT_STREAM (stream));

  return valent_mux_connection_write (self->muxer,
                                      self->uuid,
                                      buffer,
                                      count,
                                      cancellable,
                                      error);
}

/*
 * GObject
 */
static void
valent_mux_output_stream_finalize (GObject *object)
{
  ValentMuxOutputStream *self = VALENT_MUX_OUTPUT_STREAM (object);

  g_clear_object (&self->muxer);
  g_clear_pointer (&self->uuid, g_free);

  G_OBJECT_CLASS (valent_mux_output_stream_parent_class)->finalize (object);
}

static void
valent_mux_output_stream_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  ValentMuxOutputStream *self = VALENT_MUX_OUTPUT_STREAM (object);

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
valent_mux_output_stream_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  ValentMuxOutputStream *self = VALENT_MUX_OUTPUT_STREAM (object);

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
valent_mux_output_stream_class_init (ValentMuxOutputStreamClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GOutputStreamClass *stream_class = G_OUTPUT_STREAM_CLASS (klass);

  object_class->finalize = valent_mux_output_stream_finalize;
  object_class->get_property = valent_mux_output_stream_get_property;
  object_class->set_property = valent_mux_output_stream_set_property;

  stream_class->write_fn = valent_mux_output_stream_write;

  /**
   * ValentMuxOutputStream:muxer:
   *
   * The multiplexer handling data for this stream.
   */
  properties [PROP_MUXER] =
    g_param_spec_object ("muxer", NULL, NULL,
                         VALENT_TYPE_MUX_CONNECTION,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentMuxOutputStream:uuid:
   *
   * UUID of the channel that owns this stream.
   */
  properties [PROP_UUID] =
    g_param_spec_string ("uuid", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
valent_mux_output_stream_init (ValentMuxOutputStream *self)
{
}

