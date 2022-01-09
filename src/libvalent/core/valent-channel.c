// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-channel"

#include "config.h"

#include <sys/time.h>
#include <json-glib/json-glib.h>

#include "valent-channel.h"
#include "valent-data.h"
#include "valent-debug.h"
#include "valent-macros.h"
#include "valent-packet.h"
#include "valent-task-queue.h"

#define BUFFER_SIZE 4096


/**
 * SECTION:valentchannel
 * @short_description: Base class for connections
 * @title: ValentChannel
 * @stability: Unstable
 * @include: libvalent-core.h
 *
 * The #ValentChannel object is a base class for implementations of device
 * connections in Valent. Typically these are created by a #ValentChannelService
 * implementation that wraps a #GIOStream connection before emitting
 * #ValentChannelService::channel.
 *
 * Implementations will usually implement at least valent_channel_download() and
 * valent_channel_upload() for data transfer between devices. They may also
 * implement valent_channel_store_data() for storing channel specific data when
 * a devices is paired (eg. a TLS certificate).
 */

typedef struct
{
  GIOStream       *base_stream;
  JsonNode        *identity;
  JsonNode        *peer_identity;

  /* Input Buffer */
  JsonParser      *parser;
  guint8          *buffer;
  gsize            buffer_size;
  gsize            pos;
  gsize            end;

  /* Output Buffer */
  ValentTaskQueue *output;
  JsonGenerator   *generator;
} ValentChannelPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (ValentChannel, valent_channel, G_TYPE_OBJECT)

/**
 * ValentChannelClass:
 * @get_verification_key: the virtual function pointer for valent_channel_get_verification_key()
 * @download: the virtual function pointer for valent_channel_download()
 * @upload: the virtual function pointer for valent_channel_upload()
 * @store_data: the virtual function pointer for valent_channel_store_data()
 *
 * The virtual function table for #ValentChannel.
 */

enum {
  PROP_0,
  PROP_BASE_STREAM,
  PROP_IDENTITY,
  PROP_PEER_IDENTITY,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };


/*
 * Packet Buffer
 */
static inline gsize
channel_buffer_find_lf (ValentChannelPrivate *priv,
                        gsize                *cursor_out)
{
  for (gsize cursor = *cursor_out; cursor < priv->end; cursor++)
    {
      if G_UNLIKELY (priv->buffer[cursor] == '\n')
        return cursor;
    }

  return 0;
}

static JsonNode *
valent_channel_read_packet_internal (ValentChannel  *channel,
                                     GCancellable   *cancellable,
                                     GError        **error)
{
  ValentChannelPrivate *priv = valent_channel_get_instance_private (channel);
  GInputStream *input_stream;
  gsize lf_pos;
  gsize cursor;
  const char *packet_str;
  gssize packet_len;
  g_autoptr (JsonNode) packet = NULL;

  if (priv->base_stream == NULL || g_io_stream_is_closed (priv->base_stream))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_NOT_CONNECTED,
                   "Channel is closed");
      return NULL;
    }

  input_stream = g_io_stream_get_input_stream (priv->base_stream);

  cursor = priv->pos;

  while ((lf_pos = channel_buffer_find_lf (priv, &cursor)) == 0)
    {
      gssize n_read;

      /* Compact or extend the buffer */
      if G_UNLIKELY (priv->buffer_size - priv->end == 0)
        {
          if G_LIKELY (priv->pos)
            {
              gsize n_used;

              n_used = priv->end - priv->pos;

              memmove (priv->buffer, priv->buffer + priv->pos, n_used);
              cursor = cursor - priv->pos;
              priv->end = priv->end - priv->pos;
              priv->pos = 0;
            }
          else
            {
              priv->buffer_size = priv->buffer_size * 2;
              priv->buffer = g_realloc (priv->buffer, priv->buffer_size);
            }
        }

      /* Fill the buffer */
      n_read = g_input_stream_read (input_stream,
                                    priv->buffer + priv->end,
                                    priv->buffer_size - priv->end,
                                    cancellable,
                                    error);

      /* Success; loop to check for an LF */
      if (n_read > 0)
        priv->end = priv->end + n_read;

      /* End of stream; report connection closed */
      else if (n_read == 0)
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_CONNECTION_CLOSED,
                       "Channel is closed");
          return NULL;
        }

      /* There was a genuine error */
      else
        return NULL;
    }

  /* Size the line and compact buffer */
  packet_str = (const char *)priv->buffer + priv->pos;
  packet_len = lf_pos - priv->pos;
  priv->pos = lf_pos + 1;

  /* Try to parse the line as JSON */
  if (!json_parser_load_from_data (priv->parser, packet_str, packet_len, error))
    return NULL;

  packet = json_parser_steal_root (priv->parser);

  /* Simple packet validation */
  if (!valent_packet_validate (packet, error))
    return NULL;

  return g_steal_pointer (&packet);
}

static void
valent_channel_read_packet_task (GTask        *task,
                                 gpointer      source_object,
                                 gpointer      task_data,
                                 GCancellable *cancellable)
{
  ValentChannel *channel = source_object;
  GError *error = NULL;
  JsonNode *packet;

  if (g_task_return_error_if_cancelled (task))
    return;

  packet = valent_channel_read_packet_internal (channel, cancellable, &error);

  if (packet == NULL)
    return g_task_return_error (task, error);

  g_task_return_pointer (task, packet, (GDestroyNotify)json_node_unref);
}

static gboolean
valent_channel_write_packet_internal (ValentChannel  *channel,
                                      JsonNode       *packet,
                                      GCancellable   *cancellable,
                                      GError        **error)
{
  ValentChannelPrivate *priv = valent_channel_get_instance_private (channel);
  JsonObject *root;
  g_autofree char *packet_str = NULL;
  gsize packet_len;
  GOutputStream *output_stream;

  if (priv->base_stream == NULL || g_io_stream_is_closed (priv->base_stream))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_NOT_CONNECTED,
                   "Channel is closed");
      return FALSE;
    }

  /* Simple validation */
  if (!valent_packet_validate (packet, error))
    return FALSE;

  /* Timestamp the packet (UNIX Epoch ms) */
  root = json_node_get_object (packet);
  json_object_set_int_member (root, "id", valent_timestamp_ms ());

  /* Serialize the packet to a NULL-terminated string */
  json_generator_set_root (priv->generator, packet);
  packet_str = json_generator_to_data (priv->generator, &packet_len);

  /* Replace the trailing NULL with an LF */
  packet_str[packet_len] = '\n';
  packet_len += 1;

  output_stream = g_io_stream_get_output_stream (priv->base_stream);
  return g_output_stream_write_all (output_stream,
                                    packet_str,
                                    packet_len,
                                    NULL,
                                    cancellable,
                                    error);
}

static void
valent_channel_write_packet_task (GTask        *task,
                                  gpointer      source_object,
                                  gpointer      task_data,
                                  GCancellable *cancellable)
{
  ValentChannel *channel = source_object;
  JsonNode *packet = task_data;
  GError *error = NULL;

  if (g_task_return_error_if_cancelled (task))
    return;

  if (!valent_channel_write_packet_internal (channel, packet, cancellable, &error))
    return g_task_return_error (task, error);

  g_task_return_boolean (task, TRUE);
}

static void
valent_channel_close_task (GTask        *task,
                           gpointer      source_object,
                           gpointer      task_data,
                           GCancellable *cancellable)
{
  ValentChannel *channel = source_object;
  ValentChannelPrivate *priv = valent_channel_get_instance_private (channel);
  GError *error = NULL;

  if (g_task_return_error_if_cancelled (task))
    return;

  if (!g_io_stream_close (priv->base_stream, cancellable, &error))
    return g_task_return_error (task, error);

  g_task_return_boolean (task, TRUE);
}

/* LCOV_EXCL_START */
static const char *
valent_channel_real_get_verification_key (ValentChannel *channel)
{
  return NULL;
}

static GIOStream *
valent_channel_real_download (ValentChannel  *channel,
                              JsonNode       *packet,
                              GCancellable   *cancellable,
                              GError        **error)
{
  g_set_error (error,
               G_IO_ERROR,
               G_IO_ERROR_NOT_SUPPORTED,
               "%s does not implement download()",
               G_OBJECT_TYPE_NAME (channel));
  return NULL;
}

static GIOStream *
valent_channel_real_upload (ValentChannel  *channel,
                            JsonNode       *packet,
                            GCancellable   *cancellable,
                            GError        **error)
{
  g_set_error (error,
               G_IO_ERROR,
               G_IO_ERROR_NOT_SUPPORTED,
               "%s does not implement upload()",
               G_OBJECT_TYPE_NAME (channel));
  return NULL;
}

static void
valent_channel_real_store_data (ValentChannel *channel,
                                ValentData    *data)
{
  ValentChannelPrivate *priv = valent_channel_get_instance_private (channel);
  g_autoptr (GFile) file = NULL;
  g_autofree char *json = NULL;

  g_assert (VALENT_IS_CHANNEL (channel));
  g_assert (VALENT_IS_DATA (data));

  /* Save the peer identity */
  json = json_to_string (priv->peer_identity, TRUE);
  file = valent_data_new_config_file (data, "identity.json");
  g_file_set_contents_full (g_file_peek_path (file),
                            json,
                            -1,
                            G_FILE_SET_CONTENTS_CONSISTENT,
                            0600,
                            NULL);
}
/* LCOV_EXCL_STOP */

/*
 * GObject
 */
static void
valent_channel_finalize (GObject *object)
{
  ValentChannel *self = VALENT_CHANNEL (object);
  ValentChannelPrivate *priv = valent_channel_get_instance_private (self);

  g_clear_pointer (&priv->buffer, g_free);
  g_clear_object (&priv->parser);
  g_clear_pointer (&priv->output, valent_task_queue_unref);
  g_clear_object (&priv->generator);

  g_clear_object (&priv->base_stream);
  g_clear_pointer (&priv->identity, json_node_unref);
  g_clear_pointer (&priv->peer_identity, json_node_unref);

  G_OBJECT_CLASS (valent_channel_parent_class)->finalize (object);
}

static void
valent_channel_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  ValentChannel *self = VALENT_CHANNEL (object);
  ValentChannelPrivate *priv = valent_channel_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_BASE_STREAM:
      g_value_set_object (value, priv->base_stream);
      break;

    case PROP_IDENTITY:
      g_value_set_boxed (value, priv->identity);
      break;

    case PROP_PEER_IDENTITY:
      g_value_set_boxed (value, priv->peer_identity);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_channel_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  ValentChannel *self = VALENT_CHANNEL (object);
  ValentChannelPrivate *priv = valent_channel_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_BASE_STREAM:
      priv->base_stream = g_value_dup_object (value);
      break;

    case PROP_IDENTITY:
      priv->identity = g_value_dup_boxed (value);
      break;

    case PROP_PEER_IDENTITY:
      priv->peer_identity = g_value_dup_boxed (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_channel_class_init (ValentChannelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = valent_channel_finalize;
  object_class->get_property = valent_channel_get_property;
  object_class->set_property = valent_channel_set_property;

  klass->get_verification_key = valent_channel_real_get_verification_key;
  klass->download = valent_channel_real_download;
  klass->upload = valent_channel_real_upload;
  klass->store_data = valent_channel_real_store_data;

  /**
   * ValentChannel:base-stream:
   *
   * The base #GIOStream for the channel.
   */
  properties [PROP_BASE_STREAM] =
    g_param_spec_object ("base-stream",
                         "Base Stream",
                         "Base Stream",
                         G_TYPE_IO_STREAM,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentChannel:identity:
   *
   * The identity packet sent by the #ValentChannelService. In other words, this
   * identity packet represents the local device.
   */
  properties [PROP_IDENTITY] =
    g_param_spec_boxed ("identity",
                        "Identity",
                        "Identity",
                        JSON_TYPE_NODE,
                        (G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  /**
   * ValentChannel:peer-identity:
   *
   * The identity packet sent by the peer. In other words, this identity packet
   * represents the remote device.
   */
  properties [PROP_PEER_IDENTITY] =
    g_param_spec_boxed ("peer-identity",
                        "Peer Identity",
                        "Peer Identity",
                        JSON_TYPE_NODE,
                        (G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
valent_channel_init (ValentChannel *self)
{
  ValentChannelPrivate *priv = valent_channel_get_instance_private (self);

  /* Input Buffer */
  priv->parser = json_parser_new_immutable ();
  priv->buffer_size = BUFFER_SIZE;
  priv->buffer = g_malloc0 (priv->buffer_size);

  /* Output Buffer */
  priv->output = valent_task_queue_new ();
  priv->generator = json_generator_new ();
}

/**
 * valent_channel_get_base_stream:
 * @channel: a #ValentChannel
 *
 * Gets the #GIOStream for the channel, or NULL if unset.
 *
 * Returns: (nullable) (transfer none): the base #GIOStream.
 */
GIOStream *
valent_channel_get_base_stream (ValentChannel *channel)
{
  ValentChannelPrivate *priv = valent_channel_get_instance_private (channel);

  g_return_val_if_fail (VALENT_IS_CHANNEL (channel), NULL);

  return priv->base_stream;
}

/**
 * valent_channel_get_identity:
 * @channel: A #ValentChannel
 *
 * Gets the identity packet sent by the #ValentChannelService during connection
 * negotiation.
 *
 * Returns: (transfer none): The identity
 */
JsonNode *
valent_channel_get_identity (ValentChannel *channel)
{
  ValentChannelPrivate *priv = valent_channel_get_instance_private (channel);

  g_return_val_if_fail (VALENT_IS_CHANNEL (channel), NULL);

  return priv->identity;
}

/**
 * valent_channel_get_peer_identity:
 * @channel: A #ValentChannel
 *
 * Gets the identity packet sent by the peer during connection negotiation.
 *
 * Returns: (transfer none): The peer identity
 */
JsonNode *
valent_channel_get_peer_identity (ValentChannel *channel)
{
  ValentChannelPrivate *priv = valent_channel_get_instance_private (channel);

  g_return_val_if_fail (VALENT_IS_CHANNEL (channel), NULL);

  return priv->peer_identity;
}

/**
 * valent_channel_get_verification_key: (virtual get_verification_key)
 * @channel: a #ValentChannel
 *
 * Get a verification key for the connection.
 *
 * Implementations should return a string for the user to confirm the connection
 * is being made by a particular device, similar to a Bluetooth PIN.
 *
 * Returns: (transfer none): a verification key
 */
const char *
valent_channel_get_verification_key (ValentChannel *channel)
{
  g_return_val_if_fail (VALENT_IS_CHANNEL (channel), NULL);

  return VALENT_CHANNEL_GET_CLASS (channel)->get_verification_key (channel);
}

/**
 * valent_channel_read_packet:
 * @channel: a #ValentChannel
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): a #GAsyncReadyCallback
 * @user_data: (closure): user supplied data
 *
 * Asynchronously read the next #JsonNode packet from @channel. Call
 * valent_channel_read_packet_finish() to get the result.
 */
void
valent_channel_read_packet (ValentChannel       *channel,
                            GCancellable        *cancellable,
                            GAsyncReadyCallback  callback,
                            gpointer             user_data)
{
  g_autoptr (GTask) task = NULL;

  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_CHANNEL (channel));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (channel, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_channel_read_packet);
  g_task_run_in_thread (task, valent_channel_read_packet_task);

  VALENT_EXIT;
}

/**
 * valent_channel_read_packet_finish:
 * @channel: a #ValentChannel
 * @result: a #GAsyncResult
 * @error: (nullable): a #GError
 *
 * Finishes an operation started by valent_channel_read_packet().
 *
 * Returns: (transfer full): a #JsonNode or %NULL with @error set
 */
JsonNode *
valent_channel_read_packet_finish (ValentChannel  *channel,
                                   GAsyncResult   *result,
                                   GError        **error)
{
  JsonNode *ret;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_CHANNEL (channel), FALSE);
  g_return_val_if_fail (g_task_is_valid (result, channel), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  ret = g_task_propagate_pointer (G_TASK (result), error);

  VALENT_RETURN (ret);
}

/**
 * valent_channel_write_packet:
 * @channel: a #ValentChannel
 * @packet: a #JsonNode
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): a #GAsyncReadyCallback
 * @user_data: (closure): user supplied data
 *
 * Asynchronously write the #JsonNode @packet to @channel. Call
 * valent_channel_write_packet_finish() to get the result.
 */
void
valent_channel_write_packet (ValentChannel       *channel,
                             JsonNode            *packet,
                             GCancellable        *cancellable,
                             GAsyncReadyCallback  callback,
                             gpointer             user_data)
{
  ValentChannelPrivate *priv = valent_channel_get_instance_private (channel);
  g_autoptr (GTask) task = NULL;

  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_CHANNEL (channel));
  g_return_if_fail (VALENT_IS_PACKET (packet));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (channel, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_channel_write_packet);
  g_task_set_task_data (task,
                        json_node_ref (packet),
                        (GDestroyNotify)json_node_unref);

  valent_task_queue_run (priv->output, task, valent_channel_write_packet_task);

  VALENT_EXIT;
}

/**
 * valent_channel_write_packet_finish:
 * @channel: a #ValentChannel
 * @result: a #GAsyncResult
 * @error: (nullable): a #GError
 *
 * Finishes an async operation started by valent_channel_write_packet().
 *
 * Returns: %TRUE if successful, or %FALSE with @error set
 */
gboolean
valent_channel_write_packet_finish (ValentChannel  *channel,
                                    GAsyncResult   *result,
                                    GError        **error)
{
  gboolean ret;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_CHANNEL (channel), FALSE);
  g_return_val_if_fail (g_task_is_valid (result, channel), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  ret = g_task_propagate_boolean (G_TASK (result), error);

  VALENT_RETURN (ret);
}

/**
 * valent_channel_close:
 * @channel: a #ValentChannel
 * @cancellable: (nullable): a #GCancellable
 * @error: (nullable): a #GError
 *
 * Synchronously close @channel.
 *
 * Returns: %TRUE if successful, or %FALSE with @error set
 */
gboolean
valent_channel_close (ValentChannel  *channel,
                      GCancellable   *cancellable,
                      GError        **error)
{
  ValentChannelPrivate *priv = valent_channel_get_instance_private (channel);
  g_autoptr (GTask) task = NULL;
  GMainContext *context;

  g_return_val_if_fail (VALENT_IS_CHANNEL (channel), FALSE);
  g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  if (priv->base_stream == NULL || g_io_stream_is_closed (priv->base_stream))
    return TRUE;

  task = g_task_new (channel, cancellable, NULL, NULL);
  g_task_set_source_tag (task, valent_channel_close);
  valent_task_queue_run_close (priv->output, task, valent_channel_close_task);

  context = g_task_get_context (task);

  while (!g_task_get_completed (task))
    g_main_context_iteration (context, FALSE);

  return g_task_propagate_boolean (task, error);
}

/**
 * valent_channel_close_async:
 * @channel: a #ValentChannel
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): a #GAsyncReadyCallback
 * @user_data: (closure): user supplied data
 *
 * This is the asynchronous version of valent_channel_close().
 */
void
valent_channel_close_async (ValentChannel       *channel,
                            GCancellable        *cancellable,
                            GAsyncReadyCallback  callback,
                            gpointer             user_data)
{
  ValentChannelPrivate *priv = valent_channel_get_instance_private (channel);
  g_autoptr (GTask) task = NULL;

  g_return_if_fail (VALENT_IS_CHANNEL (channel));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (channel, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_channel_close_async);

  if (priv->base_stream == NULL || g_io_stream_is_closed (priv->base_stream))
    return g_task_return_boolean (task, TRUE);

  valent_task_queue_run_close (priv->output, task, valent_channel_close_task);
}

/**
 * valent_channel_close_finish:
 * @channel: a #ValentChannel
 * @result: a #GAsyncResult
 * @error: (nullable): a #GError
 *
 * Finishes an async operation started by valent_channel_close_async().
 *
 * Returns: %TRUE if successful, or %FALSE with @error set
 */
gboolean
valent_channel_close_finish (ValentChannel  *channel,
                             GAsyncResult   *result,
                             GError        **error)
{
  g_return_val_if_fail (VALENT_IS_CHANNEL (channel), FALSE);
  g_return_val_if_fail (g_task_is_valid (result, channel), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

/**
 * valent_channel_store_data: (virtual store_data)
 * @channel: a #ValentChannel
 * @data: a #ValentData
 *
 * This is called when a device is paired, allowing implementations to store
 * data required to authenticate during later connections.
 *
 * The default implementation stores the remote identity packet, so
 * implementations should chain-up to retain that behaviour.
 */
void
valent_channel_store_data (ValentChannel *channel,
                           ValentData    *data)
{
  g_return_if_fail (VALENT_IS_CHANNEL (channel));
  g_return_if_fail (VALENT_IS_DATA (data));

  VALENT_CHANNEL_GET_CLASS (channel)->store_data (channel, data);
}

/**
 * valent_channel_download: (virtual download)
 * @channel: a #ValentChannel
 * @packet: a #JsonNode packet
 * @cancellable: (nullable): a #GCancellable
 * @error: (nullable): a #GError
 *
 * Open a connection offered by the remote device. The remote device is expected
 * to populate @packet with information to negotiate the connection (eg. port).
 *
 * Typically the #GInputStream of the result will be used to read the contents
 * of a payload, while the #GInputStream is left unused.
 *
 * Returns: (transfer full) (nullable): a #GIOStream
 */
GIOStream *
valent_channel_download (ValentChannel  *channel,
                         JsonNode       *packet,
                         GCancellable   *cancellable,
                         GError        **error)
{
  GIOStream *ret;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_CHANNEL (channel), FALSE);
  g_return_val_if_fail (VALENT_IS_PACKET (packet), FALSE);
  g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  ret = VALENT_CHANNEL_GET_CLASS (channel)->download (channel,
                                                      packet,
                                                      cancellable,
                                                      error);

  VALENT_RETURN (ret);
}

/**
 * valent_channel_upload: (virtual upload)
 * @channel: a #ValentChannel
 * @packet: a #JsonNode packet to send with payload info
 * @cancellable: (nullable): a #GCancellable
 * @error: (nullable): a #GError
 *
 * Offer a connection to the remote device. Implementations are expected to
 * populate @packet with information to negotiate the connection (eg. port).
 *
 * Typically the #GOutputStream of the result will be used to write the contents
 * of a payload, while the #GInputStream is left unused.
 *
 * Returns: (transfer full) (nullable): a #GIOStream
 */
GIOStream *
valent_channel_upload (ValentChannel  *channel,
                       JsonNode       *packet,
                       GCancellable   *cancellable,
                       GError        **error)
{
  GIOStream *ret;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_CHANNEL (channel), FALSE);
  g_return_val_if_fail (VALENT_IS_PACKET (packet), FALSE);
  g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  ret = VALENT_CHANNEL_GET_CLASS (channel)->upload (channel,
                                                    packet,
                                                    cancellable,
                                                    error);

  VALENT_RETURN (ret);
}

