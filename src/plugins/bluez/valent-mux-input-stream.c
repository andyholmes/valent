// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-mux-input-stream"

#include "config.h"

#include <gio/gio.h>

#include "valent-mux-connection.h"
#include "valent-mux-input-stream.h"


struct _ValentMuxInputStream
{
  GInputStream         parent_instance;

  ValentMuxConnection *muxer;
  char                *uuid;
};

G_DEFINE_FINAL_TYPE (ValentMuxInputStream, valent_mux_input_stream, G_TYPE_INPUT_STREAM)

typedef enum {
  PROP_MUXER = 1,
  PROP_UUID,
} ValentMuxInputStreamProperty;

static GParamSpec *properties[PROP_UUID + 1] = { NULL, };


/*
 * GInputStream
 */
static gssize
valent_mux_input_stream_read (GInputStream  *stream,
                              void          *buffer,
                              size_t         count,
                              GCancellable  *cancellable,
                              GError       **error)
{
  ValentMuxInputStream *self = VALENT_MUX_INPUT_STREAM (stream);

  g_assert (VALENT_IS_MUX_INPUT_STREAM (stream));

  return valent_mux_connection_read (self->muxer,
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
valent_mux_input_stream_finalize (GObject *object)
{
  ValentMuxInputStream *self = VALENT_MUX_INPUT_STREAM (object);

  g_clear_object (&self->muxer);
  g_clear_pointer (&self->uuid, g_free);

  G_OBJECT_CLASS (valent_mux_input_stream_parent_class)->finalize (object);
}

static void
valent_mux_input_stream_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  ValentMuxInputStream *self = VALENT_MUX_INPUT_STREAM (object);

  switch ((ValentMuxInputStreamProperty)prop_id)
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
valent_mux_input_stream_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  ValentMuxInputStream *self = VALENT_MUX_INPUT_STREAM (object);

  switch ((ValentMuxInputStreamProperty)prop_id)
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
valent_mux_input_stream_class_init (ValentMuxInputStreamClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GInputStreamClass *stream_class = G_INPUT_STREAM_CLASS (klass);

  object_class->finalize = valent_mux_input_stream_finalize;
  object_class->get_property = valent_mux_input_stream_get_property;
  object_class->set_property = valent_mux_input_stream_set_property;

  stream_class->read_fn = valent_mux_input_stream_read;

  /**
   * ValentMuxInputStream:muxer:
   *
   * The multiplexer supplying data for this stream.
   */
  properties [PROP_MUXER] =
    g_param_spec_object ("muxer", NULL, NULL,
                         VALENT_TYPE_MUX_CONNECTION,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentMuxInputStream:uuid:
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

  g_object_class_install_properties (object_class, G_N_ELEMENTS (properties), properties);
}

static void
valent_mux_input_stream_init (ValentMuxInputStream *self)
{
  self->muxer = NULL;
  self->uuid = NULL;
}

