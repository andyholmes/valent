// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-mux-connection"

#include "config.h"

#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif /* _GNU_SOURCE */

#include <unistd.h>
#include <sys/eventfd.h>

#include <glib/gprintf.h>
#include <glib-unix.h>
#include <gio/gio.h>
#include <valent.h>

#include "valent-bluez-channel.h"
#include "valent-mux-io-stream.h"

#include "valent-mux-connection.h"

#define CERTIFICATE_HEADER "-----BEGIN CERTIFICATE-----\n"
#define CERTIFICATE_FOOTER "-----END CERTIFICATE-----\n"

#define DEFAULT_BUFFER_SIZE (4096)
#define HEADER_SIZE         (19)
#define PRIMARY_UUID        "a0d0aaf4-1072-4d81-aa35-902a954b1266"
#define PROTOCOL_MIN        (1)
#define PROTOCOL_MAX        (1)


struct _ValentMuxConnection
{
  ValentObject   parent_instance;

  GIOStream     *base_stream;
  uint16_t       buffer_size;

  GHashTable    *states;
  GCancellable  *cancellable;
  unsigned int   protocol_version;

  GInputStream  *input_stream;
  GOutputStream *output_stream;
};

G_DEFINE_FINAL_TYPE (ValentMuxConnection, valent_mux_connection, VALENT_TYPE_OBJECT)

typedef enum {
  PROP_BASE_STREAM = 1,
  PROP_BUFFER_SIZE,
} ValentMuxConnectionProperty;

static GParamSpec *properties[PROP_BUFFER_SIZE + 1] = { NULL, };

/**
 * MessageType:
 * @MESSAGE_PROTOCOL: The protocol version
 * @MESSAGE_OPEN: A request to open a new multiplexed channel
 * @MESSAGE_CLOSE: A request to close a multiplexed channel
 * @MESSAGE_READ: A request for more bytes
 * @MESSAGE_WRITE: A packet of bytes
 *
 * Enumeration of multiplex message types.
 */
typedef enum
{
  MESSAGE_PROTOCOL_VERSION,
  MESSAGE_OPEN_CHANNEL,
  MESSAGE_CLOSE_CHANNEL,
  MESSAGE_READ,
  MESSAGE_WRITE
} MessageType;

/**
 * ChannelState:
 * @uuid: the channel UUID
 * @mutex: a lock for changes to the state
 * @cond: a `GCond` triggered when data can be read/written
 * @stream: a `GIOStream`
 * @buf: an input buffer
 * @len: size of the input buffer
 * @pos: data start
 * @end: data end
 * @read_free: free space in the input buffer
 * @write_free: amount of bytes that can be written
 * @eventfd: a file descriptor for notifying IO state
 *
 * A thread-safe info struct to track the state of a multiplex channel.
 *
 * Each virtual multiplex channel is tracked by the real
 * `ValentMuxConnection` as a `ChannelState`.
 */
typedef struct
{
  char      *uuid;
  GMutex     mutex;
  GCond      cond;
  GIOStream *stream;

  /* Input Buffer */
  uint8_t   *buf;
  size_t     len;
  size_t     pos;
  size_t     end;

  /* I/O State */
  uint16_t   read_free;
  uint16_t   write_free;
  int        eventfd;
} ChannelState;

static ChannelState *
channel_state_new (ValentMuxConnection *muxer,
                   const char          *uuid)
{
  ChannelState *state = NULL;

  state = g_atomic_rc_box_new0 (ChannelState);
  g_mutex_init (&state->mutex);
  g_mutex_lock (&state->mutex);
  g_cond_init (&state->cond);
  state->uuid = g_strdup (uuid);
  state->len = muxer->buffer_size;
  state->buf = g_malloc0 (state->len);
  state->stream = g_object_new (VALENT_TYPE_MUX_IO_STREAM,
                                "muxer", muxer,
                                "uuid",  uuid,
                                NULL);
  state->eventfd = eventfd (0, EFD_NONBLOCK | EFD_CLOEXEC);
  if (state->eventfd == -1)
    g_critical ("%s(): %s", G_STRFUNC, g_strerror (errno));
  g_mutex_unlock (&state->mutex);

  return state;
}

static void
channel_state_close (ChannelState *state)
{
  g_mutex_lock (&state->mutex);
  if (!g_io_stream_is_closed (state->stream))
    {
      g_io_stream_close (state->stream, NULL, NULL);
      g_clear_fd (&state->eventfd, NULL);
      g_cond_broadcast (&state->cond);
    }
  g_mutex_unlock (&state->mutex);
}

static void
channel_state_free (gpointer data)
{
  ChannelState *state = (ChannelState *)data;

  channel_state_close (state);

  g_mutex_lock (&state->mutex);
  g_clear_object (&state->stream);
  g_clear_pointer (&state->buf, g_free);
  g_clear_pointer (&state->uuid, g_free);
  g_cond_clear (&state->cond);
  g_mutex_unlock (&state->mutex);
  g_mutex_clear (&state->mutex);
}

static void
channel_state_unref (gpointer data)
{
  g_atomic_rc_box_release_full (data, channel_state_free);
}

static inline gboolean
channel_state_set_error (ChannelState  *state,
                         GCancellable  *cancellable,
                         GError       **error)
{
  if (g_io_stream_is_closed (state->stream))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_CLOSED,
                   "Channel is closed");
      return TRUE;
    }

  return g_cancellable_set_error_if_cancelled (cancellable, error);
}

static inline ChannelState *
channel_state_lookup (ValentMuxConnection  *self,
                      const char           *uuid,
                      GError              **error)
{
  ChannelState *state = NULL;
  ChannelState *ret = NULL;

  valent_object_lock (VALENT_OBJECT (self));
  state = g_hash_table_lookup (self->states, uuid);
  if (state == NULL || g_io_stream_is_closed (state->stream))
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_CLOSED,
                           g_strerror (EPIPE));
    }
  else
    {
      ret = g_atomic_rc_box_acquire (state);
    }
  valent_object_unlock (VALENT_OBJECT (self));

  return ret;
}

static inline gboolean
channel_state_notify (ChannelState  *state,
                      GError       **error)
{
  int64_t byte = 1;

  if (write (state->eventfd, &byte, sizeof (uint64_t)) == -1)
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           g_io_error_from_errno (errno),
                           g_strerror (errno));
      return FALSE;
    }
  g_cond_broadcast (&state->cond);

  return TRUE;
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (ChannelState, channel_state_unref)

/**
 * pack_header:
 * @hdr: (out): a 19-byte buffer
 * @type: a `MessageType` type
 * @size: size of the message data
 * @uuid: channel UUID
 *
 * Pack a multiplex header into @hdr.
 */
static inline void
pack_header (uint8_t     *hdr,
             MessageType  type,
             uint16_t     size,
             const char  *uuid)
{
  static const uint8_t indices[16] = {
    0, 2, 4, 6, 9, 11, 14, 16, 19, 21, 24, 26, 28, 30, 32, 34,
  };

  hdr[0] = type;
  hdr[1] = (size >> 8) & 0xff;
  hdr[2] = size & 0xff;

  for (size_t i = 0; i < G_N_ELEMENTS (indices); i++)
    {
      int hi = g_ascii_xdigit_value (uuid[indices[i] + 0]);
      int lo = g_ascii_xdigit_value (uuid[indices[i] + 1]);

      hdr[3 + i] = (hi << 4) | lo;
    }

  VALENT_NOTE ("UUID: %s, TYPE: %u, SIZE: %u", uuid, type, size);
}

/**
 * unpack_header:
 * @hdr: a 19-byte buffer
 * @type: (out): a `MessageType` type
 * @size: (out): size of the message data
 * @uuid: (out): a 37-byte buffer
 *
 * Unpack the multiplex header @hdr into @type, @size and @uuid.
 */
static inline void
unpack_header (const uint8_t *hdr,
               MessageType   *type,
               uint16_t      *size,
               char          *uuid)
{
  g_assert (type != NULL);
  g_assert (size != NULL);

  *type = hdr[0];
  *size = (uint16_t)(hdr[1] << 8 | hdr[2]);
  g_snprintf (uuid, 37,
              "%02x%02x%02x%02x-"
              "%02x%02x-%02x%02x-%02x%02x-"
              "%02x%02x%02x%02x%02x%02x",
              hdr[3], hdr[4], hdr[5], hdr[6],
              hdr[7], hdr[8], hdr[9], hdr[10], hdr[11], hdr[12],
              hdr[13], hdr[14], hdr[15], hdr[16], hdr[17], hdr[18]);

  VALENT_NOTE ("UUID: %s, TYPE: %u, SIZE: %u", uuid, *type, *size);
}

/*
 * Receive Helpers
 */
static inline gboolean
recv_header (ValentMuxConnection  *self,
             MessageType          *type,
             uint16_t             *size,
             char                 *uuid,
             GCancellable         *cancellable,
             GError              **error)
{
  uint8_t hdr[HEADER_SIZE] = { 0, };
  size_t bytes_read;
  gboolean ret;

  ret = g_input_stream_read_all (self->input_stream,
                                 hdr,
                                 sizeof (hdr),
                                 &bytes_read,
                                 cancellable,
                                 error);
  if (ret)
    unpack_header (hdr, type, size, uuid);

  return ret;
}

static inline gboolean
recv_protocol_version (ValentMuxConnection  *self,
                       GCancellable         *cancellable,
                       GError              **error)
{
  gboolean ret;
  uint16_t supported_versions[2] = { 0, };
  uint16_t min_version, max_version;

  ret = g_input_stream_read_all (self->input_stream,
                                 supported_versions,
                                 sizeof (supported_versions),
                                 NULL,
                                 cancellable,
                                 error);
  if (!ret)
    return FALSE;

  min_version = GUINT16_FROM_BE (supported_versions[0]);
  max_version = GUINT16_FROM_BE (supported_versions[1]);
  if (min_version > PROTOCOL_MAX)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_NOT_SUPPORTED,
                   "Protocol version too high (v%u)",
                   min_version);
      return FALSE;
    }

  self->protocol_version = MIN (max_version, PROTOCOL_MAX);
  VALENT_NOTE ("Using multiplexer protocol v%u", self->protocol_version);

  return TRUE;
}

static inline gboolean
recv_open_channel (ValentMuxConnection  *self,
                   const char           *uuid,
                   GCancellable         *cancellable,
                   GError              **error)
{
  gboolean ret = TRUE;

  valent_object_lock (VALENT_OBJECT (self));
  if (g_hash_table_contains (self->states, uuid))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_ADDRESS_IN_USE,
                   "Channel already open (%s)",
                   uuid);
      ret = FALSE;
    }
  else
    {
      g_autoptr (ChannelState) state = NULL;

      /* NOTE: the initial MESSAGE_READ request will be sent by
       *       valent_mux_connection_accept_channel()
       */
      state = channel_state_new (self, uuid);
      g_hash_table_replace (self->states,
                            state->uuid,
                            g_atomic_rc_box_acquire (state));
    }
  valent_object_unlock (VALENT_OBJECT (self));

  return ret;
}

static inline gboolean
recv_close_channel (ValentMuxConnection  *self,
                    const char           *uuid,
                    GCancellable         *cancellable,
                    GError              **error)
{
  g_autoptr (ChannelState) state = NULL;

  valent_object_lock (VALENT_OBJECT (self));
  if (g_hash_table_steal_extended (self->states, uuid, NULL, (void **)&state))
    channel_state_close (state);
  valent_object_unlock (VALENT_OBJECT (self));

  return TRUE;
}

static inline gboolean
recv_read (ValentMuxConnection  *self,
           const char           *uuid,
           GCancellable         *cancellable,
           GError              **error)
{
  g_autoptr (ChannelState) state = NULL;
  uint16_t size_request;
  gboolean ret;

  state = channel_state_lookup (self, uuid, error);
  if (state == NULL)
    return FALSE;

  ret = g_input_stream_read_all (self->input_stream,
                                 &size_request,
                                 sizeof (size_request),
                                 NULL,
                                 cancellable,
                                 error);
  if (ret)
    {
      g_mutex_lock (&state->mutex);
      state->write_free += GUINT16_FROM_BE (size_request);
      ret = channel_state_notify (state, error);
      VALENT_NOTE ("UUID: %s, write_free: %u", state->uuid, state->write_free);
      g_mutex_unlock (&state->mutex);
    }

  return ret;
}

static inline gboolean
recv_write (ValentMuxConnection  *self,
            const char           *uuid,
            uint16_t              size,
            GCancellable         *cancellable,
            GError              **error)
{
  g_autoptr (ChannelState) state = NULL;
  size_t n_read;
  gboolean ret;

  state = channel_state_lookup (self, uuid, error);
  if (state == NULL)
    return FALSE;

  g_mutex_lock (&state->mutex);
  if G_UNLIKELY (size > state->read_free)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_MESSAGE_TOO_LARGE,
                   "Write request size (%u) exceeds available (%u)",
                   size, state->read_free);
      g_mutex_unlock (&state->mutex);
      return FALSE;
    }

  /* Compact the buffer if necessary
   */
  if (size > state->len - state->end)
    {
      size_t n_used = state->end - state->pos;

      memmove (state->buf, state->buf + state->pos, n_used);
      state->pos = 0;
      state->end = n_used;
    }

  ret = g_input_stream_read_all (self->input_stream,
                                 &state->buf[state->end],
                                 size,
                                 &n_read,
                                 cancellable,
                                 error);
  if (ret)
    {
      state->end += n_read;
      state->read_free -= n_read;
      ret = channel_state_notify (state, error);
      VALENT_NOTE ("UUID: %s, read_free: %u", state->uuid, state->read_free);
    }
  g_mutex_unlock (&state->mutex);

  return ret;
}

static gpointer
valent_mux_connection_receive_loop (gpointer data)
{
  g_autoptr (ValentMuxConnection) self = VALENT_MUX_CONNECTION (data);
  MessageType type;
  uint16_t size;
  char uuid[37] = { 0, };
  g_autoptr (GError) error = NULL;

  while (recv_header (self, &type, &size, uuid, self->cancellable, &error))
    {
      switch ((MessageType)type)
        {
        case MESSAGE_PROTOCOL_VERSION:
          if (!recv_protocol_version (self, self->cancellable, &error))
            VALENT_GOTO (out);
          break;

        case MESSAGE_OPEN_CHANNEL:
          if (!recv_open_channel (self, uuid, self->cancellable, &error))
            VALENT_GOTO (out);
          break;

        case MESSAGE_CLOSE_CHANNEL:
          if (!recv_close_channel (self, uuid, self->cancellable, &error))
            VALENT_GOTO (out);
          break;

        case MESSAGE_READ:
          if (!recv_read (self, uuid, self->cancellable, &error))
            VALENT_GOTO (out);
          break;

        case MESSAGE_WRITE:
          if (!recv_write (self, uuid, size, self->cancellable, &error))
            VALENT_GOTO (out);
          break;

        default:
          g_set_error (&error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_ARGUMENT,
                       "Unknown message type (%u)",
                       type);
          VALENT_GOTO (out);
        }
    }

out:
  g_debug ("%s(): %s", G_STRFUNC, error->message);
  valent_mux_connection_close (self, NULL, NULL);

  return NULL;
}

static inline gboolean
send_protocol_version (ValentMuxConnection  *self,
                       GCancellable         *cancellable,
                       GError              **error)
{
  uint8_t message[HEADER_SIZE + 4] = { 0, };

  /* Pack the versions big-endian
   */
  pack_header (message, MESSAGE_PROTOCOL_VERSION, 4, PRIMARY_UUID);
  message[HEADER_SIZE + 0] = (PROTOCOL_MIN >> 8) & 0xff;
  message[HEADER_SIZE + 1] = PROTOCOL_MIN & 0xff;
  message[HEADER_SIZE + 2] = (PROTOCOL_MAX >> 8) & 0xff;
  message[HEADER_SIZE + 3] = PROTOCOL_MAX & 0xff;

  return g_output_stream_write_all (self->output_stream,
                                    &message,
                                    sizeof (message),
                                    NULL,
                                    cancellable,
                                    error);
}

static inline gboolean
send_open_channel (ValentMuxConnection  *self,
                   const char           *uuid,
                   GCancellable         *cancellable,
                   GError              **error)
{
  uint8_t message[HEADER_SIZE] = { 0, };

  pack_header (message, MESSAGE_OPEN_CHANNEL, 0, uuid);

  return g_output_stream_write_all (self->output_stream,
                                    &message,
                                    sizeof (message),
                                    NULL,
                                    cancellable,
                                    error);
}

static inline gboolean
send_close_channel (ValentMuxConnection  *self,
                    const char           *uuid,
                    GCancellable         *cancellable,
                    GError              **error)
{
  uint8_t message[HEADER_SIZE] = { 0, };

  pack_header (message, MESSAGE_CLOSE_CHANNEL, 0, uuid);

  return g_output_stream_write_all (self->output_stream,
                                    &message,
                                    sizeof (message),
                                    NULL,
                                    cancellable,
                                    error);
}

static inline gboolean
send_read (ValentMuxConnection  *self,
           const char           *uuid,
           uint16_t              size_request,
           GCancellable         *cancellable,
           GError              **error)
{
  uint8_t message[HEADER_SIZE + 2] = { 0, };

  /* Pack the request big-endian
   */
  pack_header (message, MESSAGE_READ, 2, uuid);
  message[HEADER_SIZE + 0] = (size_request >> 8) & 0xff;
  message[HEADER_SIZE + 1] = size_request & 0xff;

  return g_output_stream_write_all (self->output_stream,
                                    &message,
                                    sizeof (message),
                                    NULL,
                                    cancellable,
                                    error);
}

static inline gboolean
send_write (ValentMuxConnection  *self,
            const char           *uuid,
            uint16_t              size,
            const void           *buffer,
            GCancellable         *cancellable,
            GError              **error)
{
  uint8_t message[HEADER_SIZE] = { 0, };
  gboolean ret;

  pack_header (message, MESSAGE_WRITE, size, uuid);

  ret = g_output_stream_write_all (self->output_stream,
                                   message,
                                   sizeof (message),
                                   NULL,
                                   cancellable,
                                   error);
  if (!ret)
    return FALSE;

  return g_output_stream_write_all (self->output_stream,
                                    buffer,
                                    size,
                                    NULL,
                                    cancellable,
                                    error);
}

/*
 * ValentObject
 */
static void
valent_mux_connection_destroy (ValentObject *object)
{
  ValentMuxConnection *self = VALENT_MUX_CONNECTION (object);

  valent_mux_connection_close (self, NULL, NULL);

  VALENT_OBJECT_CLASS (valent_mux_connection_parent_class)->destroy (object);
}

/*
 * GObject
 *
 * TODO: GAsyncInitable or merge with ValentMuxChannel
 */
static void
valent_mux_connection_constructed (GObject *object)
{
  ValentMuxConnection *self = VALENT_MUX_CONNECTION (object);

  G_OBJECT_CLASS (valent_mux_connection_parent_class)->constructed (object);

  valent_object_lock (VALENT_OBJECT (self));
  g_assert (G_IS_IO_STREAM (self->base_stream));
  self->input_stream = g_io_stream_get_input_stream (self->base_stream);
  self->output_stream = g_io_stream_get_output_stream (self->base_stream);
  valent_object_unlock (VALENT_OBJECT (self));
}

static void
valent_mux_connection_finalize (GObject *object)
{
  ValentMuxConnection *self = VALENT_MUX_CONNECTION (object);

  valent_object_lock (VALENT_OBJECT (self));
  g_clear_object (&self->base_stream);
  g_clear_object (&self->cancellable);
  g_clear_pointer (&self->states, g_hash_table_unref);
  valent_object_unlock (VALENT_OBJECT (self));

  G_OBJECT_CLASS (valent_mux_connection_parent_class)->finalize (object);
}

static void
valent_mux_connection_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  ValentMuxConnection *self = VALENT_MUX_CONNECTION (object);

  switch ((ValentMuxConnectionProperty)prop_id)
    {
    case PROP_BASE_STREAM:
      g_value_set_object (value, self->base_stream);
      break;

    case PROP_BUFFER_SIZE:
      g_value_set_uint (value, self->buffer_size);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_mux_connection_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  ValentMuxConnection *self = VALENT_MUX_CONNECTION (object);

  switch ((ValentMuxConnectionProperty)prop_id)
    {
    case PROP_BASE_STREAM:
      self->base_stream = g_value_dup_object (value);
      break;

    case PROP_BUFFER_SIZE:
      self->buffer_size = g_value_get_uint (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_mux_connection_class_init (ValentMuxConnectionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentObjectClass *vobject_class = VALENT_OBJECT_CLASS (klass);

  object_class->constructed = valent_mux_connection_constructed;
  object_class->finalize = valent_mux_connection_finalize;
  object_class->get_property = valent_mux_connection_get_property;
  object_class->set_property = valent_mux_connection_set_property;

  vobject_class->destroy = valent_mux_connection_destroy;

  /**
   * ValentMuxConnection:base-stream:
   *
   * The `GIOStream` being wrapped.
   */
  properties [PROP_BASE_STREAM] =
    g_param_spec_object ("base-stream", NULL, NULL,
                         G_TYPE_IO_STREAM,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentMuxConnection:buffer-size:
   *
   * Size of the input buffer allocated to each multiplex channel.
   */
  properties [PROP_BUFFER_SIZE] =
    g_param_spec_uint ("buffer-size", NULL, NULL,
                       1024, G_MAXUINT16,
                       DEFAULT_BUFFER_SIZE,
                       (G_PARAM_READWRITE |
                        G_PARAM_CONSTRUCT_ONLY |
                        G_PARAM_EXPLICIT_NOTIFY |
                        G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, G_N_ELEMENTS (properties), properties);
}

static void
valent_mux_connection_init (ValentMuxConnection *self)
{
  valent_object_lock (VALENT_OBJECT (self));
  self->cancellable = g_cancellable_new ();
  self->protocol_version = PROTOCOL_MAX;
  self->states = g_hash_table_new_full (g_str_hash,
                                        g_str_equal,
                                        NULL,
                                        channel_state_unref);
  valent_object_unlock (VALENT_OBJECT (self));
}

/**
 * valent_mux_connection_new:
 * @base_stream: (not nullable): The base stream to wrap
 *
 * Construct a new `ValentMuxConnection` for @base_stream.
 *
 * Returns: (transfer full): a `ValentMuxConnection`
 */
ValentMuxConnection *
valent_mux_connection_new (GIOStream *base_stream)
{
  return g_object_new (VALENT_TYPE_MUX_CONNECTION,
                       "base-stream", base_stream,
                       NULL);
}

/**
 * valent_mux_connection_close:
 * @connection: a `ValentMuxConnection`
 * @cancellable: (nullable): a `GCancellable`
 * @error: (nullable): a `GError`
 *
 * Close the multiplex connection.
 *
 * Returns: %TRUE if successful, or %FALSE with @error set
 */
gboolean
valent_mux_connection_close (ValentMuxConnection  *connection,
                             GCancellable         *cancellable,
                             GError              **error)
{
  GHashTableIter iter;
  ChannelState *state;
  gboolean ret;

  VALENT_ENTRY;

  g_assert (VALENT_IS_MUX_CONNECTION (connection));

  valent_object_lock (VALENT_OBJECT (connection));
  g_cancellable_cancel (connection->cancellable);

  g_hash_table_iter_init (&iter, connection->states);
  while (g_hash_table_iter_next (&iter, NULL, (void **)&state))
    {
      g_hash_table_iter_steal (&iter);
      channel_state_close (state);
      channel_state_unref (state);
    }

  ret = g_io_stream_close (connection->base_stream, cancellable, error);
  valent_object_unlock (VALENT_OBJECT (connection));

  VALENT_RETURN (ret);
}

static void
valent_mux_connection_handshake_task (GTask        *task,
                                      gpointer      source_object,
                                      gpointer      task_data,
                                      GCancellable *cancellable)
{
  ValentMuxConnection *self = VALENT_MUX_CONNECTION (source_object);
  JsonNode *identity = task_data;
  g_autoptr (ChannelState) state = NULL;
  g_autoptr (GThread) thread = NULL;
  g_autoptr (JsonNode) peer_identity = NULL;
  g_autoptr (GIOStream) base_stream = NULL;
  g_autoptr (ValentChannel) channel = NULL;
  g_autoptr (GMainContext) context = NULL;
  g_autoptr (GTlsCertificate) certificate = NULL;
  g_autoptr (GTlsCertificate) peer_certificate = NULL;
  const char *certificate_pem = NULL;
  const char *peer_certificate_pem = NULL;
  const char *device_name = NULL;
  GError *error = NULL;

  g_assert (VALENT_IS_MUX_CONNECTION (self));
  g_assert (VALENT_IS_PACKET (identity));

  if (g_task_return_error_if_cancelled (task))
    return;

  valent_object_lock (VALENT_OBJECT (self));
  if (!send_protocol_version (self, cancellable, &error))
    {
      valent_object_unlock (VALENT_OBJECT (self));
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  /* Create the primary channel and start the receive loop
   */
  state = channel_state_new (self, PRIMARY_UUID);
  g_hash_table_replace (self->states,
                        state->uuid,
                        g_atomic_rc_box_acquire (state));

  g_mutex_lock (&state->mutex);
  if (send_read (self, state->uuid, state->len, cancellable, &error))
    {
      state->read_free = state->len;
      base_stream = g_object_ref (state->stream);
    }
  g_mutex_unlock (&state->mutex);
  valent_object_unlock (VALENT_OBJECT (self));

  if (error != NULL)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  thread = g_thread_try_new ("valent-mux-connection",
                             valent_mux_connection_receive_loop,
                             g_object_ref (self),
                             &error);
  if (thread == NULL)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  /* Exchange identities
   */
  if (!valent_packet_to_stream (g_io_stream_get_output_stream (base_stream),
                                identity,
                                cancellable,
                                &error))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  context = g_main_context_new ();
  g_main_context_push_thread_default (context);
  peer_identity = valent_packet_from_stream (g_io_stream_get_input_stream (base_stream),
                                             -1,
                                             cancellable,
                                             &error);
  g_main_context_pop_thread_default (context);
  if (peer_identity == NULL)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  valent_packet_get_string (peer_identity, "deviceName", &device_name);
  VALENT_JSON (peer_identity, device_name);

  if (valent_packet_get_string (identity, "certificate", &certificate_pem))
    {
      certificate = g_tls_certificate_new_from_pem (certificate_pem, -1, &error);
      if (certificate == NULL)
        {
          g_task_return_error (task, g_steal_pointer (&error));
          return;
        }
    }

  if (valent_packet_get_string (peer_identity, "certificate", &peer_certificate_pem))
    {
      g_autofree char *pem = NULL;

      /* Some implementations might not include the header/footer
       */
      if (!g_str_has_prefix (peer_certificate_pem, CERTIFICATE_HEADER))
        {
          pem = g_strconcat (CERTIFICATE_HEADER,
                             peer_certificate_pem,
                             CERTIFICATE_FOOTER,
                             NULL);
        }
      else
        {
          pem = g_strdup (peer_certificate_pem);
        }

      peer_certificate = g_tls_certificate_new_from_pem (pem, -1, &error);
      if (peer_certificate == NULL)
        {
          g_task_return_error (task, g_steal_pointer (&error));
          return;
        }
    }
  else
    {
      g_task_return_new_error (task,
                               G_TLS_ERROR,
                               G_TLS_ERROR_CERTIFICATE_REQUIRED,
                               "Peer failed to send TLS certificate");
      return;
    }

  channel = g_object_new (VALENT_TYPE_BLUEZ_CHANNEL,
                          "base-stream",      base_stream,
                          "certificate",      certificate,
                          "identity",         identity,
                          "peer-identity",    peer_identity,
                          "peer-certificate", peer_certificate,
                          "muxer",            self,
                          NULL);
  g_task_return_pointer (task, g_steal_pointer (&channel), g_object_unref);
}

/**
 * valent_mux_connection_handshake:
 * @connection: a `ValentMuxConnection`
 * @identity: a `JsonNode`
 * @connection: a `ValentMuxConnection`
 * @cancellable: (nullable): a `GCancellable`
 * @callback: (scope async): a `GAsyncReadyCallback`
 * @user_data: user supplied data
 *
 * Attempt to negotiate a multiplex channel on @connection. This is a two-part
 * process involving negotiating the protocol version (currently only version 1)
 * and exchanging identity packets.
 *
 * Call [class@Valent.MuxConnection.handshake_finish] to get the result.
 *
 * Returns: (transfer full): a `ValentChannel`
 */
void
valent_mux_connection_handshake (ValentMuxConnection *connection,
                                 JsonNode            *identity,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
  g_autoptr (GTask) task = NULL;

  g_return_if_fail (VALENT_IS_MUX_CONNECTION (connection));
  g_return_if_fail (VALENT_IS_PACKET (identity));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (connection, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_mux_connection_handshake);
  g_task_set_task_data (task,
                        json_node_ref (identity),
                        (GDestroyNotify)json_node_unref);
  g_task_run_in_thread (task, valent_mux_connection_handshake_task);
}

/**
 * valent_mux_connection_handshake_finish:
 * @connection: a `ValentMuxConnection`
 * @result: a `GAsyncResult`
 * @error: (nullable): a `GError`
 *
 * Finishes an operation started by [class@Valent.MuxConnection.handshake].
 *
 * Returns: (transfer full): a `ValentChannel`
 */
ValentChannel *
valent_mux_connection_handshake_finish (ValentMuxConnection  *connection,
                                        GAsyncResult         *result,
                                        GError              **error)
{
  g_return_val_if_fail (VALENT_IS_MUX_CONNECTION (connection), NULL);
  g_return_val_if_fail (g_task_is_valid (result, connection), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}

/**
 * valent_mux_connection_accept_channel:
 * @connection: a `ValentMuxConnection`
 * @uuid: a channel UUID
 * @cancellable: (nullable): a `GCancellable`
 * @error: (nullable): a `GError`
 *
 * Blocks waiting for a channel to be opened for @uuid.
 *
 * Returns: (transfer full): a `GIOStream`
 */
GIOStream *
valent_mux_connection_accept_channel (ValentMuxConnection  *connection,
                                      const char           *uuid,
                                      GCancellable         *cancellable,
                                      GError              **error)
{
  g_autoptr (ChannelState) state = NULL;
  GIOStream *ret = NULL;

  g_return_val_if_fail (VALENT_IS_MUX_CONNECTION (connection), NULL);
  g_return_val_if_fail (uuid != NULL, NULL);
  g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  /* HACK: Loop every second and check for the channel
   */
  while (!g_cancellable_set_error_if_cancelled (cancellable, error))
    {
      state = channel_state_lookup (connection, uuid, NULL);
      if (state != NULL)
        {
          g_mutex_lock (&state->mutex);
          if (send_read (connection, uuid, state->len, cancellable, error))
            {
              state->read_free += state->len;
              ret = g_object_ref (state->stream);
            }
          g_mutex_unlock (&state->mutex);
          break;
        }

      g_usleep (G_USEC_PER_SEC);
    }

  return g_steal_pointer (&ret);
}

/**
 * valent_mux_connection_close_channel:
 * @connection: a `ValentMuxConnection`
 * @uuid: a channel UUID
 * @cancellable: (nullable): a `GCancellable`
 * @error: (nullable): a `GError`
 *
 * Get the multiplex protocol version used by @connection.
 *
 * Returns: the protocol version
 */
gboolean
valent_mux_connection_close_channel (ValentMuxConnection  *connection,
                                     const char           *uuid,
                                     GCancellable         *cancellable,
                                     GError              **error)
{
  gboolean ret = TRUE;

  g_return_val_if_fail (VALENT_IS_MUX_CONNECTION (connection), FALSE);

  valent_object_lock (VALENT_OBJECT (connection));
  if (g_hash_table_remove (connection->states, uuid))
    ret = send_close_channel (connection, uuid, cancellable, error);
  valent_object_unlock (VALENT_OBJECT (connection));

  return ret;
}

/**
 * valent_mux_connection_open_channel:
 * @muxer: a `ValentMuxConnection`
 * @uuid: a channel UUID
 * @cancellable: (nullable): a `GCancellable`
 * @error: (nullable): a `GError`
 *
 * Attempt to open a muxed channel for @uuid.
 *
 * Returns: (transfer full): a `GIOStream`
 */
GIOStream *
valent_mux_connection_open_channel (ValentMuxConnection  *muxer,
                                    const char           *uuid,
                                    GCancellable         *cancellable,
                                    GError              **error)
{
  GIOStream *ret = NULL;

  g_assert (VALENT_IS_MUX_CONNECTION (muxer));
  g_assert (g_uuid_string_is_valid (uuid));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_assert (error == NULL || *error == NULL);

  valent_object_lock (VALENT_OBJECT (muxer));
  if (g_hash_table_contains (muxer->states, uuid))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_ADDRESS_IN_USE,
                   "Channel already open (%s)",
                   uuid);
    }
  else
    {
      g_autoptr (ChannelState) state = NULL;

      state = channel_state_new (muxer, uuid);
      g_hash_table_replace (muxer->states,
                            state->uuid,
                            g_atomic_rc_box_acquire (state));

      g_mutex_lock (&state->mutex);
      if (send_open_channel (muxer, uuid, cancellable, error) &&
          send_read (muxer, uuid, state->len, cancellable, error))
        {
          state->read_free = state->len;
          ret = g_object_ref (state->stream);
        }
      g_mutex_unlock (&state->mutex);
    }
  valent_object_unlock (VALENT_OBJECT (muxer));

  return g_steal_pointer (&ret);
}

/**
 * valent_mux_connection_read:
 * @connection: a `ValentMuxConnection`
 * @uuid: a channel UUID
 * @buffer: a buffer to read data into
 * @count: the number of bytes that will be read from the stream
 * @blocking: whether to do blocking or non-blocking I/O
 * @cancellable: (nullable): a `GCancellable`
 * @error: (nullable): a `GError`
 *
 * Tries to read count bytes from the channel @uuid into the buffer starting at
 * @buffer.
 *
 * If @blocking is %TRUE, this function will block during the operation,
 * otherwise it may return `G_IO_ERROR_WOULD_BLOCK`.
 *
 * This is used by `ValentMuxInputStream` to implement g_input_stream_read().
 *
 * Returns: number of bytes read, or -1 on error, or 0 on end of file
 */
gssize
valent_mux_connection_read (ValentMuxConnection  *connection,
                            const char           *uuid,
                            void                 *buffer,
                            size_t                count,
                            gboolean              blocking,
                            GCancellable         *cancellable,
                            GError              **error)
{
  g_autoptr (ChannelState) state = NULL;
  gssize read;
  size_t available;
  uint16_t size_request;

  g_assert (VALENT_IS_MUX_CONNECTION (connection));
  g_assert (g_uuid_string_is_valid (uuid));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_assert (error == NULL || *error == NULL);

  state = channel_state_lookup (connection, uuid, error);
  if (state == NULL)
    return -1;

  g_mutex_lock (&state->mutex);
  if (blocking)
    {
      while (!g_io_stream_is_closed (state->stream) && state->end <= state->pos)
        g_cond_wait (&state->cond, &state->mutex);
    }
  else if (state->end <= state->pos)
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_WOULD_BLOCK,
                           g_strerror (EAGAIN));
      g_mutex_unlock (&state->mutex);
      return -1;
    }

  if (channel_state_set_error (state, cancellable, error))
    {
      g_mutex_unlock (&state->mutex);
      return -1;
    }

  available = state->end - state->pos;
  if (count <= available)
    {
      memcpy (buffer, state->buf + state->pos, count);
      state->pos += count;
      read = count;
    }
  else
    {
      memcpy (buffer, state->buf + state->pos, available);
      state->pos = 0;
      state->end = 0;
      read = available;
    }

  size_request = state->len - (state->end - state->pos) - state->read_free;
  if ((double)size_request / (double)state->len < 0.5)
    size_request = 0;
  g_mutex_unlock (&state->mutex);

  /* Request more bytes
   */
  if (size_request > 0)
    {
      if (send_read (connection, uuid, size_request, cancellable, error))
        {
          g_mutex_lock (&state->mutex);
          state->read_free += size_request;
          VALENT_NOTE ("UUID: %s, read_free: %u", state->uuid, state->read_free);
          g_mutex_unlock (&state->mutex);
        }
    }

  return read;
}

/**
 * valent_mux_connection_write:
 * @connection: a `ValentMuxConnection`
 * @uuid: a channel UUID
 * @buffer: data to write
 * @count: size of the write
 * @blocking: whether to do blocking or non-blocking I/O
 * @cancellable: (nullable): a `GCancellable`
 * @error: (nullable): a `GError`
 *
 * Tries to write @count bytes from @buffer into the stream for @uuid.
 *
 * If @blocking is %TRUE, this function will block during the operation,
 * otherwise it may return `G_IO_ERROR_WOULD_BLOCK`.
 *
 * This is used by `ValentMuxOutputStream` to implement g_output_stream_write().
 *
 * Returns: number of bytes written, or -1 with @error set
 */
gssize
valent_mux_connection_write (ValentMuxConnection  *connection,
                             const char           *uuid,
                             const void           *buffer,
                             size_t                count,
                             gboolean              blocking,
                             GCancellable         *cancellable,
                             GError              **error)
{
  g_autoptr (ChannelState) state = NULL;
  gssize written;

  g_assert (VALENT_IS_MUX_CONNECTION (connection));
  g_assert (g_uuid_string_is_valid (uuid));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_assert (error == NULL || *error == NULL);

  state = channel_state_lookup (connection, uuid, error);
  if (state == NULL)
    return -1;

  g_mutex_lock (&state->mutex);
  if (blocking)
    {
      while (!g_io_stream_is_closed (state->stream) && state->write_free == 0)
        g_cond_wait (&state->cond, &state->mutex);
    }
  else if (state->write_free == 0)
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_WOULD_BLOCK,
                           g_strerror (EAGAIN));
      g_mutex_unlock (&state->mutex);
      return -1;
    }

  if (channel_state_set_error (state, cancellable, error))
    {
      g_mutex_unlock (&state->mutex);
      return -1;
    }

  written = MIN (count, state->write_free);
  if (send_write (connection, uuid, written, buffer, cancellable, error))
    {
      state->write_free -= written;
      VALENT_NOTE ("UUID: %s, write_free: %u", state->uuid, state->write_free);
    }
  else
    {
      written = -1;
    }
  g_mutex_unlock (&state->mutex);

  return written;
}

static gboolean
broken_dispatch (GSource     *source,
                 GSourceFunc  callback,
                 gpointer     user_data)
{
  return TRUE;
}

static GSourceFuncs broken_funcs =
{
  .dispatch = broken_dispatch,
};

typedef struct
{
  GSource       source;
  ChannelState *state;
  GIOCondition  condition;
  gpointer      eventfd_tag;
} ValentMuxerSource;

static gboolean
muxer_stream_source_prepare (GSource *source,
                             int     *timeout)
{
  ValentMuxerSource *stream_source = (ValentMuxerSource *)source;
  ChannelState *state = stream_source->state;
  gboolean ret = FALSE;

  g_mutex_lock (&state->mutex);
  if (!g_io_stream_is_closed (state->stream))
    {
      if ((stream_source->condition & G_IO_OUT) != 0)
        ret = (state->write_free > 0);
      else if ((stream_source->condition & G_IO_IN) != 0)
        ret = (state->end > state->pos);
    }
  g_mutex_unlock (&state->mutex);

  if (ret)
    *timeout = 0;

  return ret;
}

static gboolean
muxer_stream_source_check (GSource *source)
{
  ValentMuxerSource *stream_source = (ValentMuxerSource *)source;
  ChannelState *state = stream_source->state;
  gboolean ret = FALSE;
  uint64_t buf;

  g_mutex_lock (&state->mutex);
  if (!g_io_stream_is_closed (state->stream))
    {
      if ((stream_source->condition & G_IO_OUT) != 0)
        ret = (state->write_free > 0);

      if ((stream_source->condition & G_IO_IN) != 0)
        ret = (state->end > state->pos);
    }
  g_mutex_unlock (&state->mutex);

  if (read (state->eventfd, &buf, sizeof (uint64_t)) == -1)
    {
      if (errno != EAGAIN && errno != EWOULDBLOCK)
        {
          g_critical ("%s(): %s", G_STRFUNC, g_strerror (errno));
          return FALSE;
        }
    }

  return ret;
}

static gboolean
muxer_stream_source_dispatch (GSource     *source,
                              GSourceFunc  callback,
                              gpointer     user_data)
{
  if (callback != NULL)
    return callback (user_data);

  return G_SOURCE_REMOVE;
}

static void
muxer_stream_source_finalize (GSource *source)
{
  ValentMuxerSource *stream_source = (ValentMuxerSource *)source;

  g_clear_pointer (&stream_source->state, channel_state_unref);
}

static gboolean
muxer_stream_source_closure_callback (gpointer data)
{
  GClosure *closure = (GClosure *)data;
  GValue result_value = G_VALUE_INIT;
  gboolean result;

  g_value_init (&result_value, G_TYPE_BOOLEAN);

  g_closure_invoke (closure, &result_value, 0, NULL, NULL);

  result = g_value_get_boolean (&result_value);
  g_value_unset (&result_value);

  return result;
}

static GSourceFuncs muxer_stream_source_funcs =
{
  .prepare = muxer_stream_source_prepare,
  .check = muxer_stream_source_check,
  .dispatch = muxer_stream_source_dispatch,
  .finalize = muxer_stream_source_finalize,
  .closure_callback = muxer_stream_source_closure_callback,
  NULL,
};

/**
 * valent_mux_connection_create_source:
 * @connection: a `ValentMuxConnection`
 * @uuid: a channel UUID
 * @condition: a `GIOCondition`
 *
 * Create a [type@GLib.Source].
 *
 * Returns: (transfer full) (nullable): a new `GSource`
 */
GSource *
valent_mux_connection_create_source (ValentMuxConnection *connection,
                                     const char          *uuid,
                                     GIOCondition         condition)
{
  g_autoptr (ChannelState) state = NULL;
  GSource *source = NULL;
  ValentMuxerSource *stream_source;

  g_assert (VALENT_IS_MUX_CONNECTION (connection));
  g_assert (g_uuid_string_is_valid (uuid));

  state = channel_state_lookup (connection, uuid, NULL);
  if (state == NULL)
    return g_source_new (&broken_funcs, sizeof (GSource));

  source = g_source_new (&muxer_stream_source_funcs, sizeof (ValentMuxerSource));
  g_source_set_static_name (source, "ValentMuxerSource");

  stream_source = (ValentMuxerSource *) source;
  stream_source->state = g_atomic_rc_box_acquire (state);
  stream_source->condition = condition;
  stream_source->eventfd_tag = g_source_add_unix_fd (source,
                                                     state->eventfd,
                                                     G_IO_IN);

  return g_steal_pointer (&source);
}

/**
 * valent_mux_connection_condition_check:
 * @connection: a `ValentMuxConnection`
 * @uuid: a channel UUID
 * @condition: a `GIOCondition` mask to check
 *
 * Checks on the readiness of the channel for @uuid to perform operations. The
 * operations specified in @condition are checked for and masked against the
 * currently-satisfied conditions.
 *
 * It is meaningless to specify %G_IO_ERR or %G_IO_HUP in condition;
 * these conditions will always be set in the output if they are true.
 *
 * This call never blocks.
 *
 * Returns: the @GIOCondition mask of the current state
 */
GIOCondition
valent_mux_connection_condition_check (ValentMuxConnection *connection,
                                       const char          *uuid,
                                       GIOCondition         condition)
{
  g_autoptr (ChannelState) state = NULL;
  GIOCondition ret = 0;

  g_assert (VALENT_IS_MUX_CONNECTION (connection));
  g_assert (g_uuid_string_is_valid (uuid));

  state = channel_state_lookup (connection, uuid, NULL);
  if (state == NULL)
    return G_IO_ERR;

  g_mutex_lock (&state->mutex);
  if (g_io_stream_is_closed (state->stream))
    {
      ret = G_IO_ERR;
    }
  else
    {
      if ((condition & G_IO_OUT) != 0 && state->write_free > 0)
        ret |= G_IO_OUT;

      if ((condition & G_IO_IN) != 0 && state->end > state->pos)
        ret |= G_IO_IN;
    }
  g_mutex_unlock (&state->mutex);

  return ret;
}
