// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-packet-input-stream"

#include "config.h"

#include <gio/gio.h>
#include <libvalent-core.h>

#include "valent-packet.h"

#include "valent-packet-input-stream.h"


struct _ValentPacketInputStream
{
  GBufferedInputStream  parent_instance;

  unsigned int          trusted : 1;
};

G_DEFINE_FINAL_TYPE (ValentPacketInputStream, valent_packet_input_stream, G_TYPE_BUFFERED_INPUT_STREAM)

typedef enum {
  PROP_TRUSTED = 1,
} ValentPacketInputStreamProperty;

static GParamSpec *properties[PROP_TRUSTED + 1] = { NULL, };


static size_t
scan_for_packet (GBufferedInputStream *stream,
                 size_t               *offset_out)
{
  const char *buffer;
  size_t offset, count;

  g_assert (offset_out != NULL);

  offset = *offset_out;
  buffer = (const char *)g_buffered_input_stream_peek_buffer (stream, &count) + offset;
  for (size_t i = 0, len = count - offset; i < len; i++)
    {
      if G_UNLIKELY (buffer[i] == '\n')
        {
          *offset_out = (offset + i) + 1;
          return *offset_out;
        }
    }

  *offset_out = count;
  return 0;
}

static void
valent_packet_input_stream_scan_buffer (GObject      *object,
                                        GAsyncResult *result,
                                        gpointer      user_data)
{
  ValentPacketInputStream *self = VALENT_PACKET_INPUT_STREAM (object);
  GBufferedInputStream *buffer = G_BUFFERED_INPUT_STREAM (self);
  g_autoptr (GTask) task = G_TASK (g_steal_pointer (&user_data));
  size_t *n_checked = (size_t *)g_task_get_task_data (task);
  size_t packet_len;
  GError *error = NULL;

  VALENT_ENTRY;

  if (result != NULL)
    {
      gssize n_filled;

      n_filled = g_buffered_input_stream_fill_finish (buffer, result, &error);
      if (n_filled <= 0)
        {
          if (error == NULL)
            {
              g_task_return_new_error_literal (task,
                                               G_IO_ERROR,
                                               G_IO_ERROR_CLOSED,
                                               "Stream is closed");
            }
          else
            {
              g_task_return_error (task, g_steal_pointer (&error));
            }

          VALENT_EXIT;
        }
    }

  packet_len = scan_for_packet (buffer, n_checked);
  if (packet_len == 0)
    {
      GCancellable *cancellable;
      size_t buffer_size, new_size;

      buffer_size = g_buffered_input_stream_get_buffer_size (buffer);
      if (g_buffered_input_stream_get_available (buffer) == buffer_size)
        {
          if (!self->trusted || !g_size_checked_mul (&new_size, buffer_size, 2))
            {
              g_task_return_new_error (task,
                                       G_IO_ERROR,
                                       G_IO_ERROR_MESSAGE_TOO_LARGE,
                                       "Packet too large");
              VALENT_EXIT;
            }

          g_buffered_input_stream_set_buffer_size (buffer, new_size);
        }

      cancellable = g_task_get_cancellable (task);
      g_buffered_input_stream_fill_async (buffer, -1,
                                          G_PRIORITY_DEFAULT,
                                          cancellable,
                                          valent_packet_input_stream_scan_buffer,
                                          g_steal_pointer (&task));
    }
  else
    {
      g_autofree char *packet_str = NULL;
      gssize n_read;
      JsonNode *packet;

      packet_str = g_malloc (packet_len + 1);
      packet_str[packet_len] = '\0';
      n_read = g_input_stream_read (G_INPUT_STREAM (buffer),
                                    packet_str,
                                    packet_len,
                                    NULL,
                                    NULL);
      g_assert_cmpint (n_read, ==, (gssize)packet_len);

      packet = valent_packet_deserialize (packet_str, &error);
      if (packet != NULL)
        {
          g_task_return_pointer (task,
                                 g_steal_pointer (&packet),
                                 (GDestroyNotify)json_node_unref);
        }
      else
        {
          g_task_return_error (task, g_steal_pointer (&error));
        }
    }

  VALENT_EXIT;
}

/*
 * GObject
 */
static void
valent_packet_input_stream_get_property (GObject    *object,
                                         guint       prop_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
  ValentPacketInputStream *self = VALENT_PACKET_INPUT_STREAM (object);

  switch ((ValentPacketInputStreamProperty)prop_id)
    {
    case PROP_TRUSTED:
      g_value_set_boolean (value, self->trusted);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_packet_input_stream_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  ValentPacketInputStream *self = VALENT_PACKET_INPUT_STREAM (object);

  switch ((ValentPacketInputStreamProperty)prop_id)
    {
    case PROP_TRUSTED:
      valent_packet_input_stream_set_trusted (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_packet_input_stream_class_init (ValentPacketInputStreamClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = valent_packet_input_stream_get_property;
  object_class->set_property = valent_packet_input_stream_set_property;

  /**
   * ValentPacketInputStream:trusted: (getter get_trusted) (setter set_trusted)
   *
   * Whether the source is trusted.
   *
   * When a stream is marked as trusted, the internal buffer will be expanded
   * automatically until a packet can be read. Otherwise a call to
   * [method@Valent.PacketInputStream.read_packet] will return an error if the
   * buffer size is reached.
   *
   * Since: 1.0
   */
  properties [PROP_TRUSTED] =
    g_param_spec_boolean ("trusted", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE |
                           G_PARAM_CONSTRUCT |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, G_N_ELEMENTS (properties), properties);
}

static void
valent_packet_input_stream_init (ValentPacketInputStream *self)
{
}

/**
 * valent_packet_input_stream_new:
 * @base_stream: a #GInputStream.
 *
 * Creates a new packet input stream for the @base_stream.
 *
 * Returns: (transfer full): a new `ValentPacketInputStream`
 *
 * Since: 1.0
 */
ValentPacketInputStream *
valent_packet_input_stream_new (GInputStream *base_stream)
{
  ValentPacketInputStream *ret;

  VALENT_ENTRY;

  g_return_val_if_fail (G_IS_INPUT_STREAM (base_stream), NULL);

  ret = g_object_new (VALENT_TYPE_PACKET_INPUT_STREAM,
                      "base-stream",       base_stream,
                      "buffer-size",       VALENT_PACKET_DEFAULT_BUFFER_SIZE,
                      "close-base-stream", FALSE,
                      NULL);

  VALENT_RETURN (ret);
}

/**
 * valent_packet_input_stream_get_trusted:
 * @stream: a `ValentPacketInputStream`
 *
 * Whether @stream is trusted.
 *
 * Returns: %TRUE if trusted, or %FALSE
 *
 * Since: 1.0
 */
gboolean
valent_packet_input_stream_get_trusted (ValentPacketInputStream *stream)
{
  g_return_val_if_fail (VALENT_IS_PACKET_INPUT_STREAM (stream), TRUE);

  return stream->trusted;
}

/**
 * valent_packet_input_stream_set_trusted:
 * @stream: a `ValentPacketInputStream`
 * @trusted: %TRUE to mark the stream as trusted
 *
 * Whether @stream is trusted.
 *
 * When a stream is marked as trusted, the internal buffer may be expanded
 * automatically until a packet can be read. Otherwise a call to
 * [method@Valent.PacketInputStream.read_packet] will return an error if the
 * buffer size is reached.
 *
 * Since: 1.0
 */
void
valent_packet_input_stream_set_trusted (ValentPacketInputStream *stream,
                                        gboolean                 trusted)
{
  g_return_if_fail (VALENT_IS_PACKET_INPUT_STREAM (stream));

  trusted = !!trusted;
  if (stream->trusted == trusted)
    return;

  stream->trusted = trusted;
  g_object_notify (G_OBJECT (stream), "trusted");
  // FIXME: ThreadSanitizer flags a race for `properties[]`
  // g_object_notify_by_pspec (G_OBJECT (stream), properties[PROP_TRUSTED]);
}

/**
 * valent_packet_input_stream_read_packet_async:
 * @stream: a given `ValentPacketInputStream`.
 * @cancellable: (nullable): a `GCancellable`
 * @callback: (scope async): a `GAsyncReadyCallback`
 * @user_data: user supplied data
 *
 * The asynchronous version of [method@Valent.PacketInputStream.read_packet].
 * It is an error to have two outstanding calls to this function.
 *
 * When the operation is finished, @callback will be called. You
 * can then call [method@Valent.PacketInputStream.read_packet_finish] to get
 * the result of the operation.
 *
 * Since: 1.0
 */
void
valent_packet_input_stream_read_packet_async (ValentPacketInputStream *stream,
                                              GCancellable            *cancellable,
                                              GAsyncReadyCallback      callback,
                                              gpointer                 user_data)
{
  GTask *task;

  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_PACKET_INPUT_STREAM (stream));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (stream, cancellable, callback, user_data);
  g_task_set_task_data (task, g_new0 (size_t, 1), g_free);
  g_task_set_source_tag (task, valent_packet_input_stream_read_packet);
  valent_packet_input_stream_scan_buffer (G_OBJECT (stream), NULL, task);

  VALENT_EXIT;
}

/**
 * valent_packet_input_stream_read_packet_finish:
 * @stream: a given `ValentPacketInputStream`
 * @result: a `GAsyncResult`
 * @error: (nullable): a `GError`
 *
 * Finish an asynchronous call started by
 * [method@Valent.PacketInputStream.read_packet].
 *
 * Returns: (transfer full) (nullable): a KDE Connect packet,
 *   or %NULL with @error set
 *
 * Since: 1.0
 */
JsonNode *
valent_packet_input_stream_read_packet_finish (ValentPacketInputStream  *stream,
                                               GAsyncResult             *result,
                                               GError                  **error)
{
  JsonNode *ret;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_PACKET_INPUT_STREAM (stream), NULL);
  g_return_val_if_fail (g_task_is_valid (result, stream), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  ret = g_task_propagate_pointer (G_TASK (result), error);

  VALENT_RETURN (ret);
}

static void
valent_packet_input_stream_read_packet_cb (GObject      *object,
                                           GAsyncResult *result,
                                           gpointer      user_data)
{
  ValentPacketInputStream *self = (ValentPacketInputStream *)object;
  GTask *task = G_TASK (user_data);
  JsonNode *packet = NULL;
  GError *error = NULL;

  g_assert (VALENT_IS_PACKET_INPUT_STREAM (self));
  g_assert (G_IS_TASK (task));

  packet = valent_packet_input_stream_read_packet_finish (self, result, &error);
  if (packet == NULL)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  g_task_return_pointer (task,
                         g_steal_pointer (&packet),
                         (GDestroyNotify)json_node_unref);
}

/**
 * valent_packet_input_stream_read_packet:
 * @stream: a given `ValentPacketInputStream`
 * @cancellable: (nullable): a `GCancellable`
 * @error: (nullable): a `GError`
 *
 * Read a KDE Connect packet from @stream.
 *
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned.
 *
 * Returns: (transfer full) (nullable): a KDE Connect packet,
 *   or %NULL with @error set
 */
JsonNode *
valent_packet_input_stream_read_packet (ValentPacketInputStream  *stream,
                                        GCancellable             *cancellable,
                                        GError                  **error)
{
  g_autoptr (GMainContext) context = NULL;
  g_autoptr (GTask) task = NULL;
  JsonNode *ret = NULL;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_PACKET_INPUT_STREAM (stream), NULL);
  g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  task = g_task_new (NULL, NULL, NULL, NULL);
  g_task_set_source_tag (task, valent_packet_input_stream_read_packet);

  valent_packet_input_stream_read_packet_async (stream,
                                                cancellable,
                                                valent_packet_input_stream_read_packet_cb,
                                                task);

  context = g_main_context_ref_thread_default ();
  while (!g_task_get_completed (task))
    g_main_context_iteration (context, TRUE);

  ret = g_task_propagate_pointer (task, error);

  VALENT_RETURN (ret);
}

