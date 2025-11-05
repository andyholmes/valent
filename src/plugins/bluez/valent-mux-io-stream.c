// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-mux-io-stream"

#include "config.h"

#include <gio/gio.h>

#include "valent-bluez-muxer.h"
#include "valent-mux-input-stream.h"
#include "valent-mux-output-stream.h"

#include "valent-mux-io-stream.h"

struct _ValentMuxIOStream
{
  GIOStream         parent_instance;

  ValentBluezMuxer *muxer;
  char             *uuid;

  GInputStream     *input_stream;
  GOutputStream    *output_stream;
};

G_DEFINE_FINAL_TYPE (ValentMuxIOStream, valent_mux_io_stream, G_TYPE_IO_STREAM)

typedef enum {
  PROP_MUXER = 1,
  PROP_UUID,
} ValentMuxIOStreamProperty;

static GParamSpec *properties[PROP_UUID + 1] = { NULL, };

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

/*
 * GObject
 */
static void
valent_mux_io_stream_constructed (GObject *object)
{
  ValentMuxIOStream *self = VALENT_MUX_IO_STREAM (object);

  G_OBJECT_CLASS (valent_mux_io_stream_parent_class)->constructed (object);

  g_assert (VALENT_IS_BLUEZ_MUXER (self->muxer));
  g_assert (g_uuid_string_is_valid (self->uuid));

  self->input_stream = g_object_new (VALENT_TYPE_MUX_INPUT_STREAM,
                                     "muxer", self->muxer,
                                     "uuid",  self->uuid,
                                     NULL);
  self->output_stream = g_object_new (VALENT_TYPE_MUX_OUTPUT_STREAM,
                                      "muxer", self->muxer,
                                      "uuid",  self->uuid,
                                      NULL);
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

  switch ((ValentMuxIOStreamProperty)prop_id)
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

  switch ((ValentMuxIOStreamProperty)prop_id)
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

  stream_class->get_input_stream = valent_mux_io_stream_get_input_stream;
  stream_class->get_output_stream = valent_mux_io_stream_get_output_stream;

  properties [PROP_MUXER] =
    g_param_spec_object ("muxer", NULL, NULL,
                         VALENT_TYPE_BLUEZ_MUXER,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

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
valent_mux_io_stream_init (ValentMuxIOStream *self)
{
}

