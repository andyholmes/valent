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

static void   g_pollable_output_stream_iface_init (GPollableOutputStreamInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (ValentMuxOutputStream, valent_mux_output_stream, G_TYPE_OUTPUT_STREAM,
                               G_IMPLEMENT_INTERFACE (G_TYPE_POLLABLE_OUTPUT_STREAM, g_pollable_output_stream_iface_init))

typedef enum {
  PROP_MUXER = 1,
  PROP_UUID,
} ValentMuxOutputStreamProperty;

static GParamSpec *properties[PROP_UUID + 1] = { NULL, };

/*
 * GPollableOutputStream
 */
static gboolean
valent_mux_output_stream_is_writable (GPollableOutputStream *pollable)
{
  ValentMuxOutputStream *self = VALENT_MUX_OUTPUT_STREAM (pollable);
  GIOCondition condition;

  condition = valent_mux_connection_condition_check (self->muxer,
                                                     self->uuid,
                                                     G_IO_OUT);

  return (condition & G_IO_OUT) != 0;
}

static gssize
valent_mux_output_stream_write_nonblocking (GPollableOutputStream  *pollable,
                                            const void             *buffer,
                                            gsize                   count,
                                            GError                **error)
{
  ValentMuxOutputStream *self = VALENT_MUX_OUTPUT_STREAM (pollable);
  gssize ret;

  VALENT_ENTRY;

  ret = valent_mux_connection_write (self->muxer,
                                     self->uuid,
                                     buffer,
                                     count,
                                     FALSE,
                                     NULL,
                                     error);

  VALENT_RETURN (ret);
}

static GSource *
valent_mux_output_stream_create_source (GPollableOutputStream *pollable,
                                        GCancellable          *cancellable)
{
  ValentMuxOutputStream *self = VALENT_MUX_OUTPUT_STREAM (pollable);
  g_autoptr (GSource) muxer_source = NULL;

  muxer_source = valent_mux_connection_create_source (self->muxer,
                                                      self->uuid,
                                                      G_IO_OUT);

  return g_pollable_source_new_full (pollable, muxer_source, cancellable);
}

static void
g_pollable_output_stream_iface_init (GPollableOutputStreamInterface *iface)
{
  iface->is_writable = valent_mux_output_stream_is_writable;
  iface->create_source = valent_mux_output_stream_create_source;
  iface->write_nonblocking = valent_mux_output_stream_write_nonblocking;
}

/*
 * GOutputStream
 */
static gboolean
valent_mux_output_stream_close (GOutputStream  *stream,
                                GCancellable   *cancellable,
                                GError        **error)
{
  ValentMuxOutputStream *self = VALENT_MUX_OUTPUT_STREAM (stream);
  gboolean ret;

  VALENT_ENTRY;

  g_assert (VALENT_IS_MUX_OUTPUT_STREAM (stream));

  ret = valent_mux_connection_close_stream (self->muxer,
                                            self->uuid,
                                            G_IO_OUT,
                                            cancellable,
                                            error);

  VALENT_RETURN (ret);
}

static gboolean
valent_mux_output_stream_flush (GOutputStream  *stream,
                                GCancellable   *cancellable,
                                GError        **error)
{
  ValentMuxOutputStream *self = VALENT_MUX_OUTPUT_STREAM (stream);
  gboolean ret;

  VALENT_ENTRY;

  g_assert (VALENT_IS_MUX_OUTPUT_STREAM (stream));

  ret = valent_mux_connection_flush_stream (self->muxer,
                                            self->uuid,
                                            cancellable,
                                            error);

  VALENT_RETURN (ret);
}

static gssize
valent_mux_output_stream_write (GOutputStream  *stream,
                                const void     *buffer,
                                size_t          count,
                                GCancellable   *cancellable,
                                GError        **error)
{
  ValentMuxOutputStream *self = VALENT_MUX_OUTPUT_STREAM (stream);
  gssize ret;

  VALENT_ENTRY;

  g_assert (VALENT_IS_MUX_OUTPUT_STREAM (stream));

  ret = valent_mux_connection_write (self->muxer,
                                     self->uuid,
                                     buffer,
                                     count,
                                     TRUE,
                                     cancellable,
                                     error);

  VALENT_RETURN (ret);
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

  switch ((ValentMuxOutputStreamProperty)prop_id)
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

  switch ((ValentMuxOutputStreamProperty)prop_id)
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

  stream_class->close_fn = valent_mux_output_stream_close;
  stream_class->flush = valent_mux_output_stream_flush;
  stream_class->write_fn = valent_mux_output_stream_write;

  properties [PROP_MUXER] =
    g_param_spec_object ("muxer", NULL, NULL,
                         VALENT_TYPE_MUX_CONNECTION,
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
valent_mux_output_stream_init (ValentMuxOutputStream *self)
{
}

