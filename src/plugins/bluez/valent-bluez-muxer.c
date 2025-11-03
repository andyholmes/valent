// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-bluez-muxer"

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

#include "valent-bluez-muxer.h"

#define IDENTITY_BUFFER_MAX  (8192)

#define CERTIFICATE_HEADER "-----BEGIN CERTIFICATE-----\n"
#define CERTIFICATE_FOOTER "-----END CERTIFICATE-----\n"

#define DEFAULT_BUFFER_SIZE (4096)
#define HEADER_SIZE         (19)
#define PRIMARY_UUID        "a0d0aaf4-1072-4d81-aa35-902a954b1266"
#define PROTOCOL_MIN        (1)
#define PROTOCOL_MAX        (1)


struct _ValentBluezMuxer
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

G_DEFINE_FINAL_TYPE (ValentBluezMuxer, valent_bluez_muxer, VALENT_TYPE_OBJECT)

typedef enum {
  PROP_BASE_STREAM = 1,
  PROP_BUFFER_SIZE,
} ValentBluezMuxerProperty;

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
 * @buffer: an input buffer
 * @size: size of the input buffer
 * @head: data start
 * @tail: data end
 * @read_free: free space in the input buffer
 * @write_free: amount of bytes that can be written
 * @eventfd: a file descriptor for notifying IO state
 *
 * A thread-safe info struct to track the state of a multiplex channel.
 *
 * Each virtual multiplex channel is tracked by the real
 * `ValentBluezMuxer` as a `ChannelState`.
 */
typedef struct
{
  char         *uuid;
  GMutex        mutex;
  GCond         cond;
  GIOStream    *stream;

  /* Input Buffer */
  uint8_t      *buffer;
  size_t        size;
  size_t        head;
  size_t        tail;
  size_t        count;

  /* I/O State */
  uint16_t      read_free;
  uint16_t      write_free;
  int           eventfd;
  GIOCondition  condition;
} ChannelState;

static ChannelState *
channel_state_new (ValentBluezMuxer *muxer,
                   const char       *uuid)
{
  ChannelState *state = NULL;

  state = g_atomic_rc_box_new0 (ChannelState);
  g_mutex_init (&state->mutex);
  g_mutex_lock (&state->mutex);
  g_cond_init (&state->cond);
  state->uuid = g_strdup (uuid);
  state->size = muxer->buffer_size;
  state->buffer = g_malloc0 (state->size);
  state->condition = (G_IO_IN | G_IO_OUT);
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

static inline size_t
channel_state_get_writable (ChannelState *state)
{
  return state->size - state->count;
}

static void
channel_state_free (gpointer data)
{
  ChannelState *state = (ChannelState *)data;

  g_mutex_lock (&state->mutex);
  g_clear_object (&state->stream);
  g_clear_pointer (&state->buffer, g_free);
  g_clear_pointer (&state->uuid, g_free);
  g_clear_fd (&state->eventfd, NULL);
  g_cond_clear (&state->cond);
  g_mutex_unlock (&state->mutex);
  g_mutex_clear (&state->mutex);
}

static void
channel_state_unref (gpointer data)
{
  g_atomic_rc_box_release_full (data, channel_state_free);
}

static inline ChannelState *
channel_state_lookup (ValentBluezMuxer  *self,
                      const char        *uuid,
                      GError           **error)
{
  ChannelState *state = NULL;
  ChannelState *ret = NULL;

  valent_object_lock (VALENT_OBJECT (self));
  state = g_hash_table_lookup (self->states, uuid);
  if (state == NULL)
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
channel_state_notify_unlocked (ChannelState  *state,
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

static inline size_t
channel_state_read_unlocked (ChannelState *state,
                             uint8_t      *buffer,
                             size_t        count)
{
  size_t tail_chunk;

  count = MIN (count, state->count);
  if (count == 0)
    return 0;

  tail_chunk = MIN (state->size - state->head, count);
  memcpy (buffer, state->buffer + state->head, tail_chunk);
  if (count > tail_chunk)
    memcpy (buffer + tail_chunk, state->buffer, count - tail_chunk);

  state->head = (state->head + count) % state->size;
  state->count -= count;

  return count;
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
recv_header (ValentBluezMuxer  *self,
             MessageType       *type,
             uint16_t          *size,
             char              *uuid,
             GCancellable      *cancellable,
             GError           **error)
{
  uint8_t hdr[HEADER_SIZE] = { 0, };
  gboolean ret;

  ret = g_input_stream_read_all (self->input_stream,
                                 hdr,
                                 sizeof (hdr),
                                 NULL,
                                 cancellable,
                                 error);
  if (ret)
    unpack_header (hdr, type, size, uuid);

  return ret;
}

static inline gboolean
recv_protocol_version (ValentBluezMuxer  *self,
                       GCancellable      *cancellable,
                       GError           **error)
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
  if (ret)
    {
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
    }

  return ret;
}

static inline gboolean
recv_open_channel (ValentBluezMuxer  *self,
                   const char        *uuid,
                   GCancellable      *cancellable,
                   GError           **error)
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
       *       valent_bluez_muxer_channel_accept()
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
recv_close_channel (ValentBluezMuxer  *self,
                    const char        *uuid,
                    GCancellable      *cancellable,
                    GError           **error)
{
  g_autoptr (ChannelState) state = NULL;

  state = channel_state_lookup (self, uuid, NULL);
  if (state == NULL)
    return TRUE;

  /* Signify the close by setting the G_IO_HUP flag,
   * to allow any pending readers to empty the buffer.
   */
  g_mutex_lock (&state->mutex);
  state->condition |= G_IO_HUP;
  channel_state_notify_unlocked (state, NULL);
  g_mutex_unlock (&state->mutex);

  return TRUE;
}

static inline gboolean
recv_read (ValentBluezMuxer  *self,
           const char        *uuid,
           GCancellable      *cancellable,
           GError           **error)
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
      ret = channel_state_notify_unlocked (state, error);
      g_mutex_unlock (&state->mutex);
    }

  return ret;
}

static inline gboolean
recv_write (ValentBluezMuxer  *self,
            const char        *uuid,
            uint16_t           size,
            GCancellable      *cancellable,
            GError           **error)
{
  g_autoptr (ChannelState) state = NULL;
  size_t tail_free;
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
                   "Write size (%u) exceeds requested (%u)",
                   size, state->read_free);
      g_mutex_unlock (&state->mutex);
      return FALSE;
    }

  tail_free = MIN (state->size - state->tail, size);
  ret = g_input_stream_read_all (self->input_stream,
                                 &state->buffer[state->tail],
                                 tail_free,
                                 &n_read,
                                 cancellable,
                                 error);
  if (ret && size > tail_free)
    {
      ret = g_input_stream_read_all (self->input_stream,
                                     &state->buffer[0],
                                     size - tail_free,
                                     &n_read,
                                     cancellable,
                                     error);
      n_read += tail_free;
    }

  if (ret)
    {
      state->tail = (state->tail + n_read) % state->size;
      state->count += n_read;
      state->read_free -= n_read;
      ret = channel_state_notify_unlocked (state, error);
    }
  g_mutex_unlock (&state->mutex);

  return ret;
}

static gpointer
valent_bluez_muxer_receive_loop (gpointer data)
{
  g_autoptr (ValentBluezMuxer) self = VALENT_BLUEZ_MUXER (data);
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
  valent_bluez_muxer_close (self, NULL, NULL);

  return NULL;
}

static inline gboolean
send_protocol_version (ValentBluezMuxer  *self,
                       GCancellable      *cancellable,
                       GError           **error)
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
send_open_channel (ValentBluezMuxer  *self,
                   const char        *uuid,
                   GCancellable      *cancellable,
                   GError           **error)
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
send_close_channel (ValentBluezMuxer  *self,
                    const char        *uuid,
                    GCancellable      *cancellable,
                    GError           **error)
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
send_read (ValentBluezMuxer  *self,
           const char        *uuid,
           uint16_t           size_request,
           GCancellable      *cancellable,
           GError           **error)
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
send_write (ValentBluezMuxer  *self,
            const char        *uuid,
            uint16_t           size,
            const void        *buffer,
            GCancellable      *cancellable,
            GError           **error)
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
valent_bluez_muxer_destroy (ValentObject *object)
{
  ValentBluezMuxer *self = VALENT_BLUEZ_MUXER (object);

  valent_bluez_muxer_close (self, NULL, NULL);

  VALENT_OBJECT_CLASS (valent_bluez_muxer_parent_class)->destroy (object);
}

/*
 * GObject
 *
 * TODO: GAsyncInitable or merge with ValentMuxChannel
 */
static void
valent_bluez_muxer_constructed (GObject *object)
{
  ValentBluezMuxer *self = VALENT_BLUEZ_MUXER (object);

  G_OBJECT_CLASS (valent_bluez_muxer_parent_class)->constructed (object);

  valent_object_lock (VALENT_OBJECT (self));
  g_assert (G_IS_IO_STREAM (self->base_stream));
  self->input_stream = g_io_stream_get_input_stream (self->base_stream);
  self->output_stream = g_io_stream_get_output_stream (self->base_stream);
  valent_object_unlock (VALENT_OBJECT (self));
}

static void
valent_bluez_muxer_finalize (GObject *object)
{
  ValentBluezMuxer *self = VALENT_BLUEZ_MUXER (object);

  valent_object_lock (VALENT_OBJECT (self));
  g_clear_object (&self->base_stream);
  g_clear_object (&self->cancellable);
  g_clear_pointer (&self->states, g_hash_table_unref);
  valent_object_unlock (VALENT_OBJECT (self));

  G_OBJECT_CLASS (valent_bluez_muxer_parent_class)->finalize (object);
}

static void
valent_bluez_muxer_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  ValentBluezMuxer *self = VALENT_BLUEZ_MUXER (object);

  switch ((ValentBluezMuxerProperty)prop_id)
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
valent_bluez_muxer_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  ValentBluezMuxer *self = VALENT_BLUEZ_MUXER (object);

  switch ((ValentBluezMuxerProperty)prop_id)
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
valent_bluez_muxer_class_init (ValentBluezMuxerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentObjectClass *vobject_class = VALENT_OBJECT_CLASS (klass);

  object_class->constructed = valent_bluez_muxer_constructed;
  object_class->finalize = valent_bluez_muxer_finalize;
  object_class->get_property = valent_bluez_muxer_get_property;
  object_class->set_property = valent_bluez_muxer_set_property;

  vobject_class->destroy = valent_bluez_muxer_destroy;

  /**
   * ValentBluezMuxer:base-stream:
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
   * ValentBluezMuxer:buffer-size:
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
valent_bluez_muxer_init (ValentBluezMuxer *self)
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
 * valent_bluez_muxer_new:
 * @base_stream: (not nullable): The base stream to wrap
 *
 * Construct a new `ValentBluezMuxer` for @base_stream.
 *
 * Returns: (transfer full): a `ValentBluezMuxer`
 */
ValentBluezMuxer *
valent_bluez_muxer_new (GIOStream *base_stream)
{
  return g_object_new (VALENT_TYPE_BLUEZ_MUXER,
                       "base-stream", base_stream,
                       NULL);
}

/**
 * valent_bluez_muxer_close:
 * @muxer: a `ValentBluezMuxer`
 * @cancellable: (nullable): a `GCancellable`
 * @error: (nullable): a `GError`
 *
 * Close the multiplex connection.
 *
 * Returns: %TRUE if successful, or %FALSE with @error set
 */
gboolean
valent_bluez_muxer_close (ValentBluezMuxer  *muxer,
                          GCancellable      *cancellable,
                          GError           **error)
{
  GHashTableIter iter;
  ChannelState *state;
  gboolean ret;

  VALENT_ENTRY;

  g_assert (VALENT_IS_BLUEZ_MUXER (muxer));

  valent_object_lock (VALENT_OBJECT (muxer));
  g_cancellable_cancel (muxer->cancellable);

  g_hash_table_iter_init (&iter, muxer->states);
  while (g_hash_table_iter_next (&iter, NULL, (void **)&state))
    {
      g_mutex_lock (&state->mutex);
      state->condition |= G_IO_HUP;
      channel_state_notify_unlocked (state, NULL);
      g_mutex_unlock (&state->mutex);
      g_hash_table_iter_remove (&iter);
    }

  ret = g_io_stream_close (muxer->base_stream, cancellable, error);
  valent_object_unlock (VALENT_OBJECT (muxer));

  VALENT_RETURN (ret);
}

typedef enum
{
  HANDSHAKE_ENCRYPTED =     (1 << 0),
  HANDSHAKE_IDENTITY_READ = (1 << 1),
  HANDSHAKE_IDENTITY_SENT = (1 << 2),
  HANDSHAKE_FAILED =        (1 << 3),
  HANDSHAKE_COMPLETE =      (HANDSHAKE_ENCRYPTED |
                             HANDSHAKE_IDENTITY_READ |
                             HANDSHAKE_IDENTITY_SENT),
} HandshakeFlags;

typedef struct
{
  GIOStream      *connection;
  JsonNode       *identity;
  JsonNode       *peer_identity;
  HandshakeFlags  flags;
} HandshakeData;

static void
handshake_data_free (gpointer user_data)
{
  HandshakeData *data = (HandshakeData *)user_data;

  g_clear_object (&data->connection);
  g_clear_pointer (&data->identity, json_node_unref);
  g_clear_pointer (&data->peer_identity, json_node_unref);
  g_free (data);
}

static void
handshake_task_complete (GTask *task)
{
  ValentBluezMuxer *self = g_task_get_source_object (task);
  HandshakeData *data = g_task_get_task_data (task);
  g_autoptr (ValentChannel) channel = NULL;
  g_autoptr (GTlsCertificate) certificate = NULL;
  g_autoptr (GTlsCertificate) peer_certificate = NULL;
  const char *certificate_pem;
  const char *peer_certificate_pem;
  GError *error = NULL;

  if (valent_packet_get_string (data->identity, "certificate", &certificate_pem))
    {
      certificate = g_tls_certificate_new_from_pem (certificate_pem, -1, &error);
      if (certificate == NULL)
        {
          g_task_return_error (task, g_steal_pointer (&error));
          return;
        }
    }

  if (valent_packet_get_string (data->peer_identity, "certificate", &peer_certificate_pem))
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
                          "base-stream",      data->connection,
                          "certificate",      certificate,
                          "identity",         data->identity,
                          "peer-certificate", peer_certificate,
                          "peer-identity",    data->peer_identity,
                          "muxer",            self,
                          NULL);
  g_task_return_pointer (task, g_object_ref (channel), g_object_unref);
}

static void
handshake_read_identity_cb (GInputStream *stream,
                            GAsyncResult *result,
                            gpointer      user_data)
{
  g_autoptr (GTask) task = G_TASK (g_steal_pointer (&user_data));
  HandshakeData *data = g_task_get_task_data (task);
  g_autoptr (JsonNode) secure_identity = NULL;
  GError *error = NULL;

  secure_identity = valent_packet_from_stream_finish (stream, result, &error);
  if (secure_identity == NULL)
    {
      if ((data->flags & HANDSHAKE_FAILED) == 0)
        {
          data->flags |= HANDSHAKE_FAILED;
          g_task_return_error (task, g_steal_pointer (&error));
        }

      return;
    }

  g_clear_pointer (&data->peer_identity, json_node_unref);
  data->peer_identity = g_steal_pointer (&secure_identity);

  data->flags |= HANDSHAKE_IDENTITY_READ;
  if (data->flags == HANDSHAKE_COMPLETE)
    handshake_task_complete (task);
}

static void
handshake_write_identity_cb (GOutputStream *stream,
                             GAsyncResult  *result,
                             gpointer       user_data)
{
  g_autoptr (GTask) task = G_TASK (g_steal_pointer (&user_data));
  HandshakeData *data = g_task_get_task_data (task);
  GError *error = NULL;

  if (!valent_packet_to_stream_finish (stream, result, &error))
    {
      if ((data->flags & HANDSHAKE_FAILED) == 0)
        {
          data->flags |= HANDSHAKE_FAILED;
          g_task_return_error (task, g_steal_pointer (&error));
        }

      return;
    }

  data->flags |= HANDSHAKE_IDENTITY_SENT;
  if (data->flags == HANDSHAKE_COMPLETE)
    handshake_task_complete (task);
}

static void
handshake_protocol_task_cb (GObject      *object,
                            GAsyncResult *result,
                            gpointer      user_data)
{
  g_autoptr (GTask) task = G_TASK (g_steal_pointer (&user_data));
  ValentBluezMuxer *self = g_task_get_source_object (task);
  HandshakeData *data = g_task_get_task_data (task);
  GCancellable *cancellable = g_task_get_cancellable (task);
  g_autoptr (GIOStream) stream = NULL;
  g_autoptr (GThread) thread = NULL;
  GError *error = NULL;

  stream = g_task_propagate_pointer (G_TASK (result), &error);
  if (stream == NULL)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  thread = g_thread_try_new ("valent-bluez-muxer",
                             valent_bluez_muxer_receive_loop,
                             g_object_ref (self),
                             &error);
  if (thread == NULL)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  data->connection = g_object_ref (stream);
  valent_packet_to_stream (g_io_stream_get_output_stream (data->connection),
                           data->identity,
                           cancellable,
                           (GAsyncReadyCallback)handshake_write_identity_cb,
                           g_object_ref (task));
  valent_packet_from_stream (g_io_stream_get_input_stream (data->connection),
                             IDENTITY_BUFFER_MAX,
                             cancellable,
                             (GAsyncReadyCallback)handshake_read_identity_cb,
                             g_object_ref (task));
}

static void
handshake_protocol_task (GTask        *task,
                         gpointer      source_object,
                         gpointer      task_data,
                         GCancellable *cancellable)
{
  ValentBluezMuxer *self = VALENT_BLUEZ_MUXER (source_object);
  ChannelState *state = (ChannelState *)task_data;
  g_autoptr (GIOStream) base_stream = NULL;
  GError *error = NULL;

  /* First send the protocol version, then request data for
   * the primary
   * multiplexed channel for the identity exchange
   */
  valent_object_lock (VALENT_OBJECT (self));
  if (!send_protocol_version (self, cancellable, &error))
    {
      valent_object_unlock (VALENT_OBJECT (self));
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  g_mutex_lock (&state->mutex);
  if (send_read (self, state->uuid, state->size, cancellable, &error))
    {
      state->read_free = state->size;
      base_stream = g_object_ref (state->stream);
    }
  g_mutex_unlock (&state->mutex);
  valent_object_unlock (VALENT_OBJECT (self));

  if (error != NULL)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  g_task_return_pointer (task, g_object_ref (base_stream), g_object_unref);
}

/**
 * valent_bluez_muxer_handshake:
 * @muxer: a `ValentBluezMuxer`
 * @identity: a KDE Connect identity packet
 * @cancellable: (nullable): a `GCancellable`
 * @callback: (scope async): a `GAsyncReadyCallback`
 * @user_data: user supplied data
 *
 * Attempt to negotiate a multiplex channel on @muxer. This is a two-part
 * process involving negotiating the protocol version (currently only version 1)
 * and exchanging identity packets.
 *
 * Call [class@Valent.BluezMuxer.handshake_finish] to get the result.
 *
 * Returns: (transfer full): a `ValentChannel`
 */
void
valent_bluez_muxer_handshake (ValentBluezMuxer    *muxer,
                              JsonNode            *identity,
                              GCancellable        *cancellable,
                              GAsyncReadyCallback  callback,
                              gpointer             user_data)
{
  g_autoptr (ChannelState) state = NULL;
  g_autoptr (GTask) protocol = NULL;
  g_autoptr (GTask) task = NULL;
  HandshakeData *data = NULL;

  g_return_if_fail (VALENT_IS_BLUEZ_MUXER (muxer));
  g_return_if_fail (VALENT_IS_PACKET (identity));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  valent_object_lock (VALENT_OBJECT (muxer));
  state = channel_state_new (muxer, PRIMARY_UUID);
  g_hash_table_replace (muxer->states,
                        state->uuid,
                        g_atomic_rc_box_acquire (state));
  valent_object_unlock (VALENT_OBJECT (muxer));

  data = g_new0 (HandshakeData, 1);
  data->identity = json_node_ref (identity);
  data->flags |= HANDSHAKE_ENCRYPTED;

  task = g_task_new (muxer, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_bluez_muxer_handshake);
  g_task_set_task_data (task, g_steal_pointer (&data), handshake_data_free);

  protocol = g_task_new (muxer, cancellable, handshake_protocol_task_cb, g_object_ref (task));
  g_task_set_source_tag (protocol, handshake_protocol_task);
  g_task_set_task_data (protocol, g_steal_pointer (&state), channel_state_unref);
  g_task_run_in_thread (protocol, handshake_protocol_task);
}

/**
 * valent_bluez_muxer_handshake_finish:
 * @muxer: a `ValentBluezMuxer`
 * @result: a `GAsyncResult`
 * @error: (nullable): a `GError`
 *
 * Finishes an operation started by [class@Valent.BluezMuxer.handshake].
 *
 * Returns: (transfer full): a `ValentChannel`
 */
ValentChannel *
valent_bluez_muxer_handshake_finish (ValentBluezMuxer  *muxer,
                                     GAsyncResult      *result,
                                     GError           **error)
{
  g_return_val_if_fail (VALENT_IS_BLUEZ_MUXER (muxer), NULL);
  g_return_val_if_fail (g_task_is_valid (result, muxer), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}

/**
 * valent_bluez_muxer_channel_accept:
 * @muxer: a `ValentBluezMuxer`
 * @uuid: a channel UUID
 * @cancellable: (nullable): a `GCancellable`
 * @error: (nullable): a `GError`
 *
 * Blocks waiting for a channel to be opened for @uuid.
 *
 * Returns: (transfer full): a `GIOStream`
 */
GIOStream *
valent_bluez_muxer_channel_accept (ValentBluezMuxer  *muxer,
                                   const char        *uuid,
                                   GCancellable      *cancellable,
                                   GError           **error)
{
  g_autoptr (ChannelState) state = NULL;
  GIOStream *ret = NULL;

  g_return_val_if_fail (VALENT_IS_BLUEZ_MUXER (muxer), NULL);
  g_return_val_if_fail (uuid != NULL, NULL);
  g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  /* HACK: Loop every second and check for the channel
   */
  while (!g_cancellable_set_error_if_cancelled (cancellable, error))
    {
      state = channel_state_lookup (muxer, uuid, NULL);
      if (state != NULL)
        {
          g_mutex_lock (&state->mutex);
          if (send_read (muxer, uuid, state->size, cancellable, error))
            {
              state->read_free += state->size;
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
 * valent_bluez_muxer_channel_close:
 * @muxer: a `ValentBluezMuxer`
 * @uuid: a channel UUID
 * @condition: a `GIOCondition`
 * @cancellable: (nullable): a `GCancellable`
 * @error: (nullable): a `GError`
 *
 * Close the stream for the channel with @uuid associated with the
 * condition for @condition.
 *
 * Returns: %TRUE, or %FALSE with @error set
 */
gboolean
valent_bluez_muxer_channel_close (ValentBluezMuxer  *muxer,
                                  const char        *uuid,
                                  GIOCondition       condition,
                                  GCancellable      *cancellable,
                                  GError           **error)
{
  g_autoptr (ChannelState) state = NULL;
  gboolean ret = TRUE;

  g_return_val_if_fail (VALENT_IS_BLUEZ_MUXER (muxer), FALSE);
  g_return_val_if_fail (uuid != NULL && *uuid != '\0', FALSE);
  g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), TRUE);

  state = channel_state_lookup (muxer, uuid, error);
  if (state == NULL)
    return FALSE;

  /* When `close()` is called on a substream unset the condition flag
   * and ensure the peer is notified of the closure
   */
  g_mutex_lock (&state->mutex);
  state->condition &= ~condition;
  if ((state->condition & G_IO_HUP) == 0)
    {
      valent_object_lock (VALENT_OBJECT (muxer));
      send_close_channel (muxer, uuid, cancellable, NULL);
      valent_object_unlock (VALENT_OBJECT (muxer));
      state->condition |= G_IO_HUP;
      ret = channel_state_notify_unlocked (state, error);
    }
  g_mutex_unlock (&state->mutex);

  return ret;
}

/**
 * valent_bluez_muxer_channel_flush:
 * @muxer: a `ValentBluezMuxer`
 * @uuid: a channel UUID
 * @cancellable: (nullable): a `GCancellable`
 * @error: (nullable): a `GError`
 *
 * Flush the streams by notifying any pollable sources or waiting threads
 * of a condition change.
 *
 * Returns: %TRUE, or %FALSE with @error set
 */
gboolean
valent_bluez_muxer_channel_flush (ValentBluezMuxer  *muxer,
                                  const char        *uuid,
                                  GCancellable      *cancellable,
                                  GError           **error)
{
  g_autoptr (ChannelState) state = NULL;
  gboolean ret;

  g_return_val_if_fail (VALENT_IS_BLUEZ_MUXER (muxer), FALSE);
  g_return_val_if_fail (uuid != NULL && *uuid != '\0', FALSE);
  g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), TRUE);

  state = channel_state_lookup (muxer, uuid, error);
  if (state == NULL)
    return FALSE;

  g_mutex_lock (&state->mutex);
  ret = channel_state_notify_unlocked (state, error);
  g_mutex_unlock (&state->mutex);

  return ret;
}

/**
 * valent_bluez_muxer_channel_open:
 * @muxer: a `ValentBluezMuxer`
 * @uuid: a channel UUID
 * @cancellable: (nullable): a `GCancellable`
 * @error: (nullable): a `GError`
 *
 * Attempt to open a muxed channel for @uuid.
 *
 * Returns: (transfer full): a `GIOStream`
 */
GIOStream *
valent_bluez_muxer_channel_open (ValentBluezMuxer  *muxer,
                                 const char        *uuid,
                                 GCancellable      *cancellable,
                                 GError           **error)
{
  GIOStream *ret = NULL;

  g_assert (VALENT_IS_BLUEZ_MUXER (muxer));
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
          send_read (muxer, uuid, state->size, cancellable, error))
        {
          state->read_free = state->size;
          ret = g_object_ref (state->stream);
        }
      g_mutex_unlock (&state->mutex);
    }
  valent_object_unlock (VALENT_OBJECT (muxer));

  return g_steal_pointer (&ret);
}

/**
 * valent_bluez_muxer_read:
 * @muxer: a `ValentBluezMuxer`
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
valent_bluez_muxer_read (ValentBluezMuxer  *muxer,
                         const char        *uuid,
                         void              *buffer,
                         size_t             count,
                         gboolean           blocking,
                         GCancellable      *cancellable,
                         GError           **error)
{
  g_autoptr (ChannelState) state = NULL;
  gssize read;
  uint16_t size_request = 0;

  g_assert (VALENT_IS_BLUEZ_MUXER (muxer));
  g_assert (g_uuid_string_is_valid (uuid));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_assert (error == NULL || *error == NULL);

  state = channel_state_lookup (muxer, uuid, error);
  if (state == NULL)
    return -1;

  g_mutex_lock (&state->mutex);
  if ((state->condition & G_IO_IN) == 0)
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_CLOSED,
                           g_strerror (EPIPE));
      g_mutex_unlock (&state->mutex);
      return -1;
    }

  if ((state->condition & G_IO_HUP) != 0)
    {
      /* Return buffer contents before signaling EOF
       */
      if (state->count > 0)
        {
          read = channel_state_read_unlocked (state, buffer, count);
          g_mutex_unlock (&state->mutex);
          return read;
        }

      /* Signal EOF and mark as closed
       */
      state->condition &= ~G_IO_IN;
      g_mutex_unlock (&state->mutex);
      return 0;
    }

  if (blocking)
    {
      while ((state->condition & G_IO_HUP) == 0 && state->count == 0)
        g_cond_wait (&state->cond, &state->mutex);

      if ((state->condition & G_IO_HUP) != 0)
        {
          /* Return the contents of the before signaling EOF
           */
          if (state->count > 0)
            {
              read = channel_state_read_unlocked (state, buffer, count);
              g_mutex_unlock (&state->mutex);
              return read;
            }

          /* Signal EOF and mark as closed
           */
          state->condition &= ~G_IO_IN;
          g_mutex_unlock (&state->mutex);
          return 0;
        }
    }
  else if (state->count == 0)
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_WOULD_BLOCK,
                           g_strerror (EAGAIN));
      g_mutex_unlock (&state->mutex);
      return -1;
    }

  read = channel_state_read_unlocked (state, buffer, count);
  size_request = channel_state_get_writable (state) - state->read_free;
  if ((double)size_request / (double)state->size < 0.5)
    size_request = 0;
  g_mutex_unlock (&state->mutex);

  /* Request more bytes
   */
  if (size_request > 0)
    {
      if (send_read (muxer, uuid, size_request, cancellable, error))
        {
          g_mutex_lock (&state->mutex);
          state->read_free += size_request;
          g_mutex_unlock (&state->mutex);
        }
    }

  return read;
}

/**
 * valent_bluez_muxer_write:
 * @muxer: a `ValentBluezMuxer`
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
valent_bluez_muxer_write (ValentBluezMuxer  *muxer,
                          const char        *uuid,
                          const void        *buffer,
                          size_t             count,
                          gboolean           blocking,
                          GCancellable      *cancellable,
                          GError           **error)
{
  g_autoptr (ChannelState) state = NULL;
  gssize written;

  g_assert (VALENT_IS_BLUEZ_MUXER (muxer));
  g_assert (g_uuid_string_is_valid (uuid));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_assert (error == NULL || *error == NULL);

  state = channel_state_lookup (muxer, uuid, error);
  if (state == NULL)
    return -1;

  g_mutex_lock (&state->mutex);
  if ((state->condition & G_IO_OUT) == 0)
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_CLOSED,
                           g_strerror (EPIPE));
      g_mutex_unlock (&state->mutex);
      return -1;
    }

  if (blocking)
    {
      while ((state->condition & G_IO_HUP) == 0 && state->write_free == 0)
        g_cond_wait (&state->cond, &state->mutex);

      if ((state->condition & G_IO_OUT) == 0)
        {
          g_set_error_literal (error,
                               G_IO_ERROR,
                               G_IO_ERROR_CLOSED,
                               g_strerror (EPIPE));
          g_mutex_unlock (&state->mutex);
          return -1;
        }
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

  written = MIN (count, state->write_free);
  if (send_write (muxer, uuid, written, buffer, cancellable, error))
    {
      state->write_free -= written;
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
  if ((stream_source->condition & G_IO_OUT) != 0)
    ret |= TRUE;

  if ((stream_source->condition & G_IO_OUT) != 0)
    ret |= (state->write_free > 0);

  if ((stream_source->condition & G_IO_IN) != 0)
    ret |= (state->count > 0);
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
  if ((state->condition & G_IO_HUP) != 0)
    ret = TRUE;

  if ((stream_source->condition & G_IO_IN) != 0)
    ret |= (state->count > 0);

  if ((stream_source->condition & G_IO_OUT) != 0)
    ret |= (state->write_free > 0);

  if (read (state->eventfd, &buf, sizeof (uint64_t)) == -1)
    {
      if (errno != EAGAIN && errno != EWOULDBLOCK)
        {
          g_critical ("%s(): %s", G_STRFUNC, g_strerror (errno));
          ret = FALSE;
        }
    }
  g_mutex_unlock (&state->mutex);

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
 * valent_bluez_muxer_create_source:
 * @muxer: a `ValentBluezMuxer`
 * @uuid: a channel UUID
 * @condition: a `GIOCondition`
 *
 * Create a [type@GLib.Source].
 *
 * Returns: (transfer full) (nullable): a new `GSource`
 */
GSource *
valent_bluez_muxer_create_source (ValentBluezMuxer *muxer,
                                  const char       *uuid,
                                  GIOCondition      condition)
{
  g_autoptr (ChannelState) state = NULL;
  GSource *source = NULL;
  ValentMuxerSource *stream_source;

  g_assert (VALENT_IS_BLUEZ_MUXER (muxer));
  g_assert (g_uuid_string_is_valid (uuid));

  state = channel_state_lookup (muxer, uuid, NULL);
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
 * valent_bluez_muxer_condition_check:
 * @muxer: a `ValentBluezMuxer`
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
valent_bluez_muxer_condition_check (ValentBluezMuxer *muxer,
                                    const char       *uuid,
                                    GIOCondition      condition)
{
  g_autoptr (ChannelState) state = NULL;
  GIOCondition ret = 0;

  g_assert (VALENT_IS_BLUEZ_MUXER (muxer));
  g_assert (g_uuid_string_is_valid (uuid));

  state = channel_state_lookup (muxer, uuid, NULL);
  if (state == NULL)
    return G_IO_ERR;

  g_mutex_lock (&state->mutex);
  if ((state->condition & G_IO_HUP) != 0)
    ret |= G_IO_HUP;

  if ((condition & G_IO_IN) != 0 && (state->count > 0))
    ret |= G_IO_IN;

  if ((condition & G_IO_OUT) != 0 && state->write_free > 0)
    ret |= G_IO_OUT;
  g_mutex_unlock (&state->mutex);

  return ret;
}

