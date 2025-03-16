// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-mux-connection"

#include "config.h"

#include <glib/gprintf.h>
#include <valent.h>

#include "valent-bluez-channel.h"
#include "valent-mux-connection.h"
#include "valent-mux-io-stream.h"

#define BUFFER_SIZE  4096
#define HEADER_SIZE  19
#define PRIMARY_UUID "a0d0aaf4-1072-4d81-aa35-902a954b1266"
#define PROTOCOL_MIN 1
#define PROTOCOL_MAX 1


struct _ValentMuxConnection
{
  ValentObject   parent_instance;

  GIOStream     *base_stream;
  GInputStream  *input_stream;
  GOutputStream *output_stream;
  uint16_t       buffer_size;
  GCancellable  *cancellable;

  unsigned int   protocol_version;

  GHashTable    *states;
  GMutex         io_mutex;
};

G_DEFINE_FINAL_TYPE (ValentMuxConnection, valent_mux_connection, VALENT_TYPE_OBJECT)

typedef enum {
  PROP_BASE_STREAM = 1,
  PROP_BUFFER_SIZE,
} ValentMuxConnectionProperty;

static GParamSpec *properties[PROP_BUFFER_SIZE + 1] = { NULL, };


/*
 * UUID Helpers from Linux
 */
static const uint8_t si[16] = {0,2,4,6,9,11,14,16,19,21,24,26,28,30,32,34};

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
 * @stream: a `GIOStream`
 * @buf: an input buffer
 * @len: size of the input buffer
 * @pos: data start
 * @end: data end
 * @read_free: free space in the input buffer
 * @read_cond: a `GCond` triggered when data can be read
 * @write_free: amount of bytes that can be written
 * @write_cond: a `GCond` triggered when data can be written
 *
 * A thread-safe info struct to track the state of a multiplex channel.
 *
 * Each virtual multiplex channel is tracked by the real `ValentMuxConnection` as
 * a `ChannelState`.
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
} ChannelState;

static ChannelState *
channel_state_new (ValentMuxConnection *connection,
                   const char          *uuid)
{
  ChannelState *state;

  state = g_atomic_rc_box_new0 (ChannelState);

  /* Mutex */
  g_mutex_init (&state->mutex);
  g_mutex_lock (&state->mutex);
  g_cond_init (&state->cond);

  state->uuid = g_strdup (uuid);

  /* Input Buffer */
  state->len = BUFFER_SIZE;
  state->pos = 0;
  state->end = 0;
  state->buf = g_malloc0 (state->len);

  /* I/O State */
  state->read_free = 0;
  state->write_free = 0;

  /* I/O Streams */
  state->stream = g_object_new (VALENT_TYPE_MUX_IO_STREAM,
                                "muxer", connection,
                                "uuid",  uuid,
                                NULL);

  g_mutex_unlock (&state->mutex);

  return state;
}

static void
channel_state_close (gpointer data)
{
  ChannelState *state = data;

  g_mutex_lock (&state->mutex);
  if (!g_io_stream_is_closed (state->stream))
    {
      g_io_stream_close (state->stream, NULL, NULL);
      g_cond_broadcast (&state->cond);
    }
  g_mutex_unlock (&state->mutex);
}

static void
channel_state_free (gpointer data)
{
  ChannelState *state = data;

  /* Ensure the channel is closed */
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
                   "Channel closed: %s",
                   state->uuid);
      return TRUE;
    }

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return TRUE;

  return FALSE;
}

static inline ChannelState *
channel_state_lookup (ValentMuxConnection  *self,
                      const char           *uuid,
                      GError              **error)
{
  ChannelState *state = NULL;

  valent_object_lock (VALENT_OBJECT (self));
  if ((state = g_hash_table_lookup (self->states, uuid)) == NULL)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_NOT_CONNECTED,
                   "Channel does not exist '%s'",
                   uuid);
      valent_object_unlock (VALENT_OBJECT (self));
      return NULL;
    }

  if (channel_state_set_error (state, NULL, error))
    {
      valent_object_unlock (VALENT_OBJECT (self));
      return NULL;
    }

  state = g_atomic_rc_box_acquire (state);
  valent_object_unlock (VALENT_OBJECT (self));

  return state;
}

static inline gboolean
channel_state_remove (ValentMuxConnection *self,
                      const char          *uuid)
{
  valent_object_lock (VALENT_OBJECT (self));
  g_hash_table_remove (self->states, uuid);
  valent_object_unlock (VALENT_OBJECT (self));

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
  hdr[0] = type;
  hdr[1] = (size >> 8) & 0xff;
  hdr[2] = size & 0xff;

  for (int i = 0; i < 16; i++)
    {
      int hi = g_ascii_xdigit_value (uuid[si[i] + 0]);
      int lo = g_ascii_xdigit_value (uuid[si[i] + 1]);

      hdr[i + 3] = (hi << 4) | lo;
    }
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
unpack_header (uint8_t     *hdr,
               MessageType *type,
               uint16_t    *size,
               char        *uuid)
{
  if G_LIKELY (type != NULL)
    *type = hdr[0];

  if G_LIKELY (size != NULL)
    *size = (uint16_t)hdr[1] << 8 | hdr[2];

  if G_LIKELY (uuid != NULL)
    g_snprintf (uuid, 37,
                "%02x%02x%02x%02x-"
                "%02x%02x-%02x%02x-%02x%02x-"
                "%02x%02x%02x%02x%02x%02x",
                hdr[3], hdr[4], hdr[5], hdr[6],
                hdr[7], hdr[8], hdr[9], hdr[10], hdr[11], hdr[12],
                hdr[13], hdr[14], hdr[15], hdr[16], hdr[17], hdr[18]);
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
  uint8_t hdr[HEADER_SIZE];
  size_t bytes_read;
  gboolean ret;

  ret = g_input_stream_read_all (self->input_stream,
                                 &hdr,
                                 HEADER_SIZE,
                                 &bytes_read,
                                 cancellable,
                                 error);

  if (!ret)
    return FALSE;

  unpack_header (hdr, type, size, uuid);

  VALENT_NOTE ("%s(): UUID: %s, TYPE: %u, SIZE: %u",
               G_STRFUNC, uuid, *type, *size);

  return TRUE;
}

static inline gboolean
recv_protocol_version (ValentMuxConnection  *self,
                       GCancellable         *cancellable,
                       GError              **error)
{
  gboolean ret;
  uint16_t min_version, max_version;

  ret = g_input_stream_read_all (self->input_stream,
                                 &min_version,
                                 2,
                                 NULL,
                                 cancellable,
                                 error);

  if (!ret)
    return FALSE;

  ret = g_input_stream_read_all (self->input_stream,
                                 &max_version,
                                 2,
                                 NULL,
                                 cancellable,
                                 error);

  if (!ret)
    return FALSE;

  /* Ensure byte-order */
  min_version = GUINT16_FROM_BE (min_version);
  max_version = GUINT16_FROM_BE (max_version);

  if (min_version > PROTOCOL_MAX)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_NOT_SUPPORTED,
                   "Protocol version too high (v%u)",
                   min_version);
      return FALSE;
    }

  /* Accept the highest supported version */
  self->protocol_version = MIN (max_version, PROTOCOL_MAX);

  return TRUE;
}

static inline gboolean
recv_open_channel (ValentMuxConnection  *self,
                   const char           *uuid,
                   GCancellable         *cancellable,
                   GError              **error)
{
  ChannelState *state;
  gboolean ret;

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
      state = channel_state_new (self, uuid);
      g_hash_table_insert (self->states, state->uuid, state);
      ret = TRUE;
    }
  valent_object_unlock (VALENT_OBJECT (self));

  return ret;
}

static inline gboolean
recv_close_channel (ValentMuxConnection  *connection,
                    const char           *uuid,
                    GCancellable         *cancellable,
                    GError              **error)
{
  return channel_state_remove (connection, uuid);
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

  ret = g_input_stream_read_all (self->input_stream,
                                 &size_request,
                                 2,
                                 NULL,
                                 cancellable,
                                 error);

  if (!ret)
    return FALSE;

  /* Update the state and signal waiting threads */
  if ((state = channel_state_lookup (self, uuid, NULL)) != NULL)
    {
      valent_object_lock (VALENT_OBJECT (self));
      g_mutex_lock (&state->mutex);
      state->write_free += GUINT16_FROM_BE (size_request);
      VALENT_NOTE ("write_free: %u", state->write_free);
      g_cond_broadcast (&state->cond);
      g_mutex_unlock (&state->mutex);
      valent_object_unlock (VALENT_OBJECT (self));
    }

  return TRUE;
}

static inline gboolean
recv_write (ValentMuxConnection  *self,
            const char           *uuid,
            uint16_t              size,
            GCancellable         *cancellable,
            GError              **error)
{
  g_autoptr (ChannelState) state = NULL;
  size_t buf_used;
  gboolean ret;

  /* Ensure this channel exists */
  if ((state = channel_state_lookup (self, uuid, error)) == NULL)
    return FALSE;

  valent_object_lock (VALENT_OBJECT (self));
  g_mutex_lock (&state->mutex);

  /* Avoid buffer overflow */
  if G_UNLIKELY (size > state->read_free)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_MESSAGE_TOO_LARGE,
                   "Write request size (%u) exceeds available (%u)",
                   size, state->read_free);
      g_mutex_unlock (&state->mutex);
      valent_object_unlock (VALENT_OBJECT (self));
      return FALSE;
    }

  /* Compact the buffer if necessary */
  buf_used = state->end - state->pos;

  if (size > state->len - state->end)
    {
      memmove (state->buf, state->buf + state->pos, buf_used);
      state->pos = 0;
      state->end = buf_used;
    }

  /* Read directly into the buffer */
  ret = g_input_stream_read_all (self->input_stream,
                                 &state->buf[state->end],
                                 size,
                                 NULL,
                                 cancellable,
                                 error);

  if (!ret)
    {
      g_mutex_unlock (&state->mutex);
      valent_object_unlock (VALENT_OBJECT (self));
      return FALSE;
    }

  /* Notify waiting threads */
  state->end += size;
  state->read_free -= size;
  VALENT_NOTE ("read_free: %u (-%u)", state->read_free, size);
  g_cond_broadcast (&state->cond);
  g_mutex_unlock (&state->mutex);
  valent_object_unlock (VALENT_OBJECT (self));

  return TRUE;
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
      switch (type)
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

  return NULL;
}

static inline gboolean
send_protocol_version (ValentMuxConnection  *self,
                       GCancellable         *cancellable,
                       GError              **error)
{
  uint8_t message[HEADER_SIZE + 4] = { 0, };

  /* Pack the versions big-endian */
  pack_header (message, MESSAGE_PROTOCOL_VERSION, 4, PRIMARY_UUID);
  message[HEADER_SIZE + 0] = (PROTOCOL_MIN >> 8) & 0xff;
  message[HEADER_SIZE + 1] = PROTOCOL_MIN & 0xff;
  message[HEADER_SIZE + 2] = (PROTOCOL_MAX >> 8) & 0xff;
  message[HEADER_SIZE + 3] = PROTOCOL_MAX & 0xff;

  return g_output_stream_write_all (self->output_stream,
                                    &message,
                                    HEADER_SIZE + 4,
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
                                    HEADER_SIZE,
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
                                    HEADER_SIZE,
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

  /* Pack the message */
  pack_header (message, MESSAGE_READ, 2, uuid);
  message[HEADER_SIZE + 0] = (size_request >> 8) & 0xff;
  message[HEADER_SIZE + 1] = size_request & 0xff;

  /* Write the message */
  return g_output_stream_write_all (self->output_stream,
                                    &message,
                                    HEADER_SIZE + 2,
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
  uint8_t hdr[HEADER_SIZE] = { 0, };
  gboolean ret;

  /* Pack the header */
  pack_header (hdr, MESSAGE_WRITE, size, uuid);

  /* Write the header */
  ret = g_output_stream_write_all (self->output_stream,
                                   hdr,
                                   HEADER_SIZE,
                                   NULL,
                                   cancellable,
                                   error);

  if (!ret)
    return FALSE;

  /* Write the data */
  return g_output_stream_write_all (self->output_stream,
                                    buffer,
                                    size,
                                    NULL,
                                    cancellable,
                                    error);
}

/*
 * Handshake Helpers
 */
static gboolean
protocol_handshake (ValentMuxConnection  *self,
                    GCancellable         *cancellable,
                    GError              **error)
{
  MessageType type;
  uint16_t size;

  /* Send our protocol min/max */
  if (!send_protocol_version (self, cancellable, error))
    return FALSE;

  /* Receive the header */
  if (!recv_header (self, &type, &size, NULL, cancellable, error))
    return FALSE;

  /* Which must be a protocol version */
  if (type != MESSAGE_PROTOCOL_VERSION)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_ARGUMENT,
                   "Expected PROTOCOL_VERSION (0), got (%u)",
                   type);
      return FALSE;
    }

  /* Choose the best version */
  if (!recv_protocol_version (self, cancellable, error))
    return FALSE;

  return TRUE;
}

/*
 * ValentObject
 */
static void
valent_mux_connection_destroy (ValentObject *object)
{
  ValentMuxConnection *self = VALENT_MUX_CONNECTION (object);
  GSocket *socket;

  if (!g_cancellable_is_cancelled (self->cancellable))
    g_cancellable_cancel (self->cancellable);

  socket = g_socket_connection_get_socket (G_SOCKET_CONNECTION (self->base_stream));
  g_socket_close (socket, NULL);

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

  g_assert (G_IS_IO_STREAM (self->base_stream));

  G_OBJECT_CLASS (valent_mux_connection_parent_class)->constructed (object);

  valent_object_lock (VALENT_OBJECT (self));
  self->input_stream = g_io_stream_get_input_stream (self->base_stream);
  self->output_stream = g_io_stream_get_output_stream (self->base_stream);
  valent_object_unlock (VALENT_OBJECT (self));
}

static void
valent_mux_connection_finalize (GObject *object)
{
  ValentMuxConnection *self = VALENT_MUX_CONNECTION (object);

  /* Close all sub-streams */
  g_clear_pointer (&self->states, g_hash_table_unref);
  g_clear_object (&self->base_stream);
  g_clear_object (&self->cancellable);

  g_mutex_clear (&self->io_mutex);

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
   * The "base-stream" property is the `GIOStream` being wrapped.
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
                       BUFFER_SIZE,
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
  g_mutex_init (&self->io_mutex);
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
                       "buffer-size", BUFFER_SIZE,
                       NULL);
}

static void
valent_mux_connection_handshake_task (GTask        *task,
                                      gpointer      source_object,
                                      gpointer      task_data,
                                      GCancellable *cancellable)
{
  ValentMuxConnection *self = VALENT_MUX_CONNECTION (source_object);
  JsonNode *identity = task_data;
  ChannelState *state;
  GInputStream *input_stream;
  GOutputStream *output_stream;
  g_autoptr (GThread) thread = NULL;
  g_autoptr (JsonNode) peer_identity = NULL;
  g_autoptr (GIOStream) base_stream = NULL;
  g_autoptr (ValentChannel) channel = NULL;
  g_autoptr (GError) error = NULL;

  g_assert (VALENT_IS_MUX_CONNECTION (self));
  g_assert (VALENT_IS_PACKET (identity));

  if (g_task_return_error_if_cancelled (task))
    return;

  /* Create the primary channel */
  valent_object_lock (VALENT_OBJECT (self));
  state = channel_state_new (self, PRIMARY_UUID);
  g_hash_table_insert (self->states, state->uuid, state);
  base_stream = g_object_ref (state->stream);
  valent_object_unlock (VALENT_OBJECT (self));

  /* Negotiate protocol version */
  if (!protocol_handshake (self, cancellable, &error))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  /* Send an initial read request and start the receive loop  */
  g_mutex_lock (&self->io_mutex);
  if (send_read (self, PRIMARY_UUID, BUFFER_SIZE, cancellable, &error))
    {
      g_mutex_lock (&state->mutex);
      state->read_free += BUFFER_SIZE;
      g_mutex_unlock (&state->mutex);
    }
  else
    {
      g_mutex_unlock (&self->io_mutex);
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }
  g_mutex_unlock (&self->io_mutex);

  thread = g_thread_try_new ("valent-mux-connection",
                             valent_mux_connection_receive_loop,
                             g_object_ref (self),
                             &error);

  if (thread == NULL)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  /* Write our identity
   */
  output_stream = g_io_stream_get_output_stream (base_stream);
  if (!valent_packet_to_stream (output_stream, identity, cancellable, &error))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  /* Read the remote identity */
  input_stream = g_io_stream_get_input_stream (base_stream);
  peer_identity = valent_packet_from_stream (input_stream,
                                             -1,
                                             cancellable,
                                             &error);

  if (peer_identity == NULL)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  channel = g_object_new (VALENT_TYPE_BLUEZ_CHANNEL,
                          "base-stream",   base_stream,
                          "identity",      identity,
                          "peer-identity", peer_identity,
                          "muxer",         self,
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

  g_return_val_if_fail (VALENT_IS_MUX_CONNECTION (connection), NULL);
  g_return_val_if_fail (uuid != NULL, NULL);
  g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  /* HACK: Loop every second and check for the channel */
  while (!g_cancellable_set_error_if_cancelled (cancellable, error))
    {
      if ((state = channel_state_lookup (connection, uuid, NULL)) != NULL)
        {
          if (!send_read (connection, uuid, BUFFER_SIZE, cancellable, error))
            return NULL;
          state->read_free += BUFFER_SIZE;

          return g_object_ref (state->stream);
        }

      g_usleep (G_USEC_PER_SEC);
    }

  return NULL;
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
  g_return_val_if_fail (VALENT_IS_MUX_CONNECTION (connection), FALSE);

  if (!g_cancellable_is_cancelled (connection->cancellable))
    g_cancellable_cancel (connection->cancellable);

  return g_io_stream_close (connection->base_stream, cancellable, error);
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
  gboolean ret;

  g_return_val_if_fail (VALENT_IS_MUX_CONNECTION (connection), FALSE);

  /* Drop the channel state */
  if (!channel_state_remove (connection, uuid))
    return TRUE;

  /* Inform the peer of closure */
  g_mutex_lock (&connection->io_mutex);
  ret = send_close_channel (connection, uuid, cancellable, error);
  g_mutex_unlock (&connection->io_mutex);

  return ret;
}

/**
 * valent_mux_connection_open_channel:
 * @connection: a `ValentMuxConnection`
 * @uuid: a channel UUID
 * @cancellable: (nullable): a `GCancellable`
 * @error: (nullable): a `GError`
 *
 * Attempt to open a muxed channel for @uuid.
 *
 * Returns: (transfer full): a `GIOStream`
 */
GIOStream *
valent_mux_connection_open_channel (ValentMuxConnection  *connection,
                                    const char           *uuid,
                                    GCancellable         *cancellable,
                                    GError              **error)
{
  g_autoptr (ChannelState) state = NULL;

  g_assert (VALENT_IS_MUX_CONNECTION (connection));
  g_assert (g_uuid_string_is_valid (uuid));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_assert (error == NULL || *error == NULL);

  /* Ensure the channel doesn't already exist */
  valent_object_lock (VALENT_OBJECT (connection));
  if (g_hash_table_contains (connection->states, uuid))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_ADDRESS_IN_USE,
                   "Channel already open (%s)",
                   uuid);
      return NULL;
    }
  valent_object_unlock (VALENT_OBJECT (connection));

  /* Inform the peer we're opening a channel */
  g_mutex_lock (&connection->io_mutex);
  if (!send_open_channel (connection, uuid, cancellable, error))
    {
      g_mutex_unlock (&connection->io_mutex);
      return NULL;
    }
  g_mutex_unlock (&connection->io_mutex);

  /* Track the new channel */
  valent_object_lock (VALENT_OBJECT (connection));
  state = channel_state_new (connection, uuid);
  g_hash_table_insert (connection->states, state->uuid, state);
  valent_object_unlock (VALENT_OBJECT (connection));

  return g_object_ref (state->stream);
}

/**
 * valent_mux_connection_read:
 * @connection: a `ValentMuxConnection`
 * @uuid: a channel UUID
 * @buffer: a buffer to read data into
 * @count: the number of bytes that will be read from the stream
 * @cancellable: (nullable): a `GCancellable`
 * @error: (nullable): a `GError`
 *
 * Tries to read count bytes from the channel @uuid into the buffer starting at
 * @buffer. Will block during this read.
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
                            GCancellable         *cancellable,
                            GError              **error)
{
  g_autoptr (ChannelState) state = NULL;
  gssize read;
  size_t n_used;

  g_assert (VALENT_IS_MUX_CONNECTION (connection));
  g_assert (g_uuid_string_is_valid (uuid));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_assert (error == NULL || *error == NULL);

  /* Ensure the channel exists */
  if ((state = channel_state_lookup (connection, uuid, error)) == NULL)
    return -1;

  /* Block for available data */
  valent_object_lock (VALENT_OBJECT (connection));
  g_mutex_lock (&state->mutex);

  while (!g_io_stream_is_closed (state->stream) && state->end - state->pos < 1)
    {
      valent_object_unlock (VALENT_OBJECT (connection));
      g_cond_wait (&state->cond, &state->mutex);
      valent_object_lock (VALENT_OBJECT (connection));
    }

  if (channel_state_set_error (state, cancellable, error))
    {
      g_mutex_unlock (&state->mutex);
      valent_object_unlock (VALENT_OBJECT (connection));
      return -1;
    }

  /* Steal <= count from the buffer */
  n_used = state->end - state->pos;

  if (count <= n_used)
    {
      memcpy (buffer, state->buf + state->pos, count);
      state->pos += count;
      read = count;
    }
  else
    {
      memcpy (buffer, state->buf + state->pos, n_used);
      state->pos = 0;
      state->end = 0;
      read = n_used;
    }
  g_mutex_unlock (&state->mutex);
  valent_object_unlock (VALENT_OBJECT (connection));

  /* Request more bytes */
  g_mutex_lock (&connection->io_mutex);
  if (send_read (connection, uuid, read, cancellable, error))
    {
      g_mutex_lock (&state->mutex);
      state->read_free += read;
      VALENT_NOTE ("read_free: %u", state->read_free);
      g_mutex_unlock (&state->mutex);
    }
  g_mutex_unlock (&connection->io_mutex);

  return read;
}

/**
 * valent_mux_connection_write:
 * @connection: a `ValentMuxConnection`
 * @uuid: a channel UUID
 * @buffer: data to write
 * @count: size of the write
 * @cancellable: (nullable): a `GCancellable`
 * @error: (nullable): a `GError`
 *
 * Tries to write @count bytes from @buffer into the stream for @uuid. Will
 * block during the operation.
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
                             GCancellable         *cancellable,
                             GError              **error)
{
  g_autoptr (ChannelState) state = NULL;
  gssize written;

  g_assert (VALENT_IS_MUX_CONNECTION (connection));
  g_assert (g_uuid_string_is_valid (uuid));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_assert (error == NULL || *error == NULL);

  /* Ensure the channel exists */
  if ((state = channel_state_lookup (connection, uuid, error)) == NULL)
    return -1;

  /* Wait for available write space */
  valent_object_lock (VALENT_OBJECT (connection));
  g_mutex_lock (&connection->io_mutex);

  while (!g_io_stream_is_closed (state->stream) && state->write_free == 0)
    {
      valent_object_unlock (VALENT_OBJECT (connection));
      g_cond_wait (&state->cond, &connection->io_mutex);
      valent_object_lock (VALENT_OBJECT (connection));
    }

  if (channel_state_set_error (state, cancellable, error))
    {
      g_mutex_unlock (&connection->io_mutex);
      valent_object_unlock (VALENT_OBJECT (connection));
      return -1;
    }

  /* Write the data */
  written = MIN (count, state->write_free);

  if (!send_write (connection, uuid, written, buffer, cancellable, error))
    {
      g_mutex_unlock (&connection->io_mutex);
      valent_object_unlock (VALENT_OBJECT (connection));
      return -1;
    }
  g_mutex_unlock (&connection->io_mutex);

  /* Update the state */
  g_mutex_lock (&state->mutex);
  state->write_free -= written;
  VALENT_NOTE ("write_free: %u", state->write_free);
  g_mutex_unlock (&state->mutex);
  valent_object_unlock (VALENT_OBJECT (connection));

  return written;
}

