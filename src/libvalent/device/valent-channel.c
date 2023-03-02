// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-channel"

#include "config.h"

#include <time.h>

#include <gio/gio.h>
#include <json-glib/json-glib.h>
#include <libvalent-core.h>

#include "valent-channel.h"
#include "valent-packet.h"


/**
 * ValentChannel:
 *
 * A base class for device connections.
 *
 * #ValentChannel is a base class for the primary communication channel in
 * Valent. It is effectively an abstraction layer around a [class@Gio.IOStream].
 *
 * ## Packet Exchange
 *
 * The core of the KDE Connect protocol is built on the exchange of JSON
 * packets, similar to JSON-RPC. Packets can be queued concurrently from
 * different threads with [method@Valent.Channel.write_packet] and read
 * sequentially with [method@Valent.Channel.read_packet].
 *
 * Packets may contain payload information, allowing devices to negotiate
 * auxiliary connections. Incoming connections can be accepted by passing the
 * packet to [method@Valent.Channel.download], or opened by passing the packet
 * to [method@Valent.Channel.upload].
 *
 * ## Implementation Notes
 *
 * Implementations should override [vfunc@Valent.Channel.download] and
 * [vfunc@Valent.Channel.upload] to support accepting and opening auxiliary
 * connections, respectively. If pairing involves exchanging a key, override
 * [vfunc@Valent.Channel.get_verification_key]. To know when to store persistent
 * data related to the connection, override [vfunc@Valent.Channel.store_data].
 *
 * Since: 1.0
 */

typedef struct
{
  GIOStream        *base_stream;
  JsonNode         *identity;
  JsonNode         *peer_identity;

  /* Packet Buffer */
  GDataInputStream *input_buffer;
  GQueue            output_buffer;
  unsigned int      output_pending : 1;
} ValentChannelPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (ValentChannel, valent_channel, VALENT_TYPE_OBJECT)

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

static void
valent_channel_real_download_task (GTask        *task,
                                   gpointer      source_object,
                                   gpointer      task_data,
                                   GCancellable *cancellable)
{
  ValentChannel *self = source_object;
  JsonNode *packet = task_data;
  g_autoptr (GIOStream) stream = NULL;
  GError *error = NULL;

  if (g_task_return_error_if_cancelled (task))
    return;

  stream = VALENT_CHANNEL_GET_CLASS (self)->download (self,
                                                      packet,
                                                      cancellable,
                                                      &error);

  if (stream == NULL)
    return g_task_return_error (task, error);

  g_task_return_pointer (task, g_steal_pointer (&stream), g_object_unref);
}

static void
valent_channel_real_download_async (ValentChannel       *channel,
                                    JsonNode            *packet,
                                    GCancellable        *cancellable,
                                    GAsyncReadyCallback  callback,
                                    gpointer             user_data)
{
  g_autoptr (GTask) task = NULL;

  g_assert (VALENT_IS_CHANNEL (channel));
  g_assert (VALENT_IS_PACKET (packet));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (channel, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_channel_real_download_async);
  g_task_set_task_data (task,
                        json_node_ref (packet),
                        (GDestroyNotify)json_node_unref);
  g_task_run_in_thread (task, valent_channel_real_download_task);
}

static GIOStream *
valent_channel_real_download_finish (ValentChannel  *channel,
                                     GAsyncResult   *result,
                                     GError        **error)
{
  g_assert (VALENT_IS_CHANNEL (channel));
  g_assert (g_task_is_valid (result, channel));
  g_assert (error == NULL || *error == NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
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
valent_channel_upload_task (GTask        *task,
                            gpointer      source_object,
                            gpointer      task_data,
                            GCancellable *cancellable)
{
  ValentChannel *self = source_object;
  JsonNode *packet = task_data;
  g_autoptr (GIOStream) stream = NULL;
  GError *error = NULL;

  if (g_task_return_error_if_cancelled (task))
    return;

  stream = VALENT_CHANNEL_GET_CLASS (self)->upload (self,
                                                    packet,
                                                    cancellable,
                                                    &error);

  if (stream == NULL)
    return g_task_return_error (task, error);

  g_task_return_pointer (task, g_steal_pointer (&stream), g_object_unref);
}

static void
valent_channel_real_upload_async (ValentChannel       *channel,
                                  JsonNode            *packet,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data)
{
  g_autoptr (GTask) task = NULL;

  g_assert (VALENT_IS_CHANNEL (channel));
  g_assert (VALENT_IS_PACKET (packet));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (channel, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_channel_real_upload_async);
  g_task_set_task_data (task,
                        json_node_ref (packet),
                        (GDestroyNotify)json_node_unref);
  g_task_run_in_thread (task, valent_channel_upload_task);
}

static GIOStream *
valent_channel_real_upload_finish (ValentChannel  *channel,
                                   GAsyncResult   *result,
                                   GError        **error)
{
  g_assert (VALENT_IS_CHANNEL (channel));
  g_assert (g_task_is_valid (result, channel));
  g_assert (error == NULL || *error == NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
valent_channel_real_store_data (ValentChannel *channel,
                                ValentContext *context)
{
  g_assert (VALENT_IS_CHANNEL (channel));
  g_assert (VALENT_IS_CONTEXT (context));
}
/* LCOV_EXCL_STOP */


/*
 * ValentChannel
 */
static inline void
valent_channel_write_cancel (gpointer data)
{
  g_autoptr (GTask) task = G_TASK (data);

  if (!g_task_get_completed (task))
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_CONNECTION_CLOSED,
                               "Channel is closed");
    }
}

static inline gboolean
valent_channel_return_error_if_closed (ValentChannel *self,
                                       GTask         *task)
{
  ValentChannelPrivate *priv = valent_channel_get_instance_private (self);

  if (g_task_return_error_if_cancelled (task))
    return TRUE;

  valent_object_lock (VALENT_OBJECT (self));
  if (priv->base_stream == NULL || g_io_stream_is_closed (priv->base_stream))
    {
      g_queue_clear_full (&priv->output_buffer, valent_channel_write_cancel);
      g_clear_object (&priv->input_buffer);
      valent_object_unlock (VALENT_OBJECT (self));

      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_CONNECTION_CLOSED,
                               "Channel is closed");
      return TRUE;
    }

  return FALSE;
}

static void
valent_channel_set_base_stream (ValentChannel *self,
                                GIOStream     *base_stream)
{
  ValentChannelPrivate *priv = valent_channel_get_instance_private (self);

  g_assert (VALENT_IS_CHANNEL (self));

  if (base_stream != NULL)
    {
      GInputStream *input_stream;

      valent_object_lock (VALENT_OBJECT (self));
      input_stream = g_io_stream_get_input_stream (base_stream);

      priv->base_stream = g_object_ref (base_stream);
      priv->input_buffer = g_object_new (G_TYPE_DATA_INPUT_STREAM,
                                         "base-stream",       input_stream,
                                         "close-base-stream", FALSE,
                                         NULL);
      g_queue_init (&priv->output_buffer);
      valent_object_unlock (VALENT_OBJECT (self));
    }
}


/*
 * GObject
 */
static void
valent_channel_finalize (GObject *object)
{
  ValentChannel *self = VALENT_CHANNEL (object);
  ValentChannelPrivate *priv = valent_channel_get_instance_private (self);

  valent_object_lock (VALENT_OBJECT (self));
  g_queue_clear (&priv->output_buffer);
  g_clear_object (&priv->input_buffer);
  g_clear_object (&priv->base_stream);
  g_clear_pointer (&priv->identity, json_node_unref);
  g_clear_pointer (&priv->peer_identity, json_node_unref);
  valent_object_unlock (VALENT_OBJECT (self));

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
      g_value_take_object (value, valent_channel_ref_base_stream (self));
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
      valent_channel_set_base_stream (self, g_value_get_object (value));
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
  klass->download_async = valent_channel_real_download_async;
  klass->download_finish = valent_channel_real_download_finish;
  klass->upload = valent_channel_real_upload;
  klass->upload_async = valent_channel_real_upload_async;
  klass->upload_finish = valent_channel_real_upload_finish;
  klass->store_data = valent_channel_real_store_data;

  /**
   * ValentChannel:base-stream: (getter ref_base_stream)
   *
   * The base [class@Gio.IOStream] for the channel.
   *
   * Implementations of [class@Valent.ChannelService] must set this property
   * during construction.
   *
   * Since: 1.0
   */
  properties [PROP_BASE_STREAM] =
    g_param_spec_object ("base-stream", NULL, NULL,
                         G_TYPE_IO_STREAM,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentChannel:identity: (getter get_identity)
   *
   * The local identity packet.
   *
   * This is the identity packet sent by the [class@Valent.ChannelService]
   * implementation to identify the host system.
   *
   * Implementations of [class@Valent.ChannelService] must set this property
   * during construction.
   *
   * Since: 1.0
   */
  properties [PROP_IDENTITY] =
    g_param_spec_boxed ("identity", NULL, NULL,
                        JSON_TYPE_NODE,
                        (G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  /**
   * ValentChannel:peer-identity: (getter get_peer_identity)
   *
   * The peer identity packet.
   *
   * This is the identity packet sent by the peer to identify itself.
   *
   * Implementations of [class@Valent.ChannelService] must set this property
   * during construction.
   *
   * Since: 1.0
   */
  properties [PROP_PEER_IDENTITY] =
    g_param_spec_boxed ("peer-identity", NULL, NULL,
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
}

/**
 * valent_channel_ref_base_stream: (get-property base-stream)
 * @channel: a #ValentChannel
 *
 * Get the base [class@Gio.IOStream].
 *
 * Returns: (transfer full) (nullable): the base stream
 *
 * Since: 1.0
 */
GIOStream *
valent_channel_ref_base_stream (ValentChannel *channel)
{
  ValentChannelPrivate *priv = valent_channel_get_instance_private (channel);
  GIOStream *ret = NULL;

  g_return_val_if_fail (VALENT_IS_CHANNEL (channel), NULL);

  valent_object_lock (VALENT_OBJECT (channel));
  if (priv->base_stream != NULL)
    ret = g_object_ref (priv->base_stream);
  valent_object_unlock (VALENT_OBJECT (channel));

  return g_steal_pointer (&ret);
}

/**
 * valent_channel_get_identity: (get-property identity)
 * @channel: A #ValentChannel
 *
 * Get the local identity packet.
 *
 * Returns: (transfer none): a KDE Connect packet
 *
 * Since: 1.0
 */
JsonNode *
valent_channel_get_identity (ValentChannel *channel)
{
  ValentChannelPrivate *priv = valent_channel_get_instance_private (channel);

  g_return_val_if_fail (VALENT_IS_CHANNEL (channel), NULL);

  return priv->identity;
}

/**
 * valent_channel_get_peer_identity: (get-property peer-identity)
 * @channel: A #ValentChannel
 *
 * Get the peer identity packet.
 *
 * Returns: (transfer none): a KDE Connect packet
 *
 * Since: 1.0
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
 * Implementations that involve exchanging a key should return a string for the
 * user to authenticate the connection, similar to a Bluetooth PIN.
 *
 * Returns: (transfer none): a verification key
 *
 * Since: 1.0
 */
const char *
valent_channel_get_verification_key (ValentChannel *channel)
{
  const char *ret;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_CHANNEL (channel), NULL);

  ret = VALENT_CHANNEL_GET_CLASS (channel)->get_verification_key (channel);

  VALENT_RETURN (ret);
}

/**
 * valent_channel_close:
 * @channel: a #ValentChannel
 * @cancellable: (nullable): a #GCancellable
 * @error: (nullable): a #GError
 *
 * Close the channel.
 *
 * Returns: %TRUE if successful, or %FALSE with @error set
 */
gboolean
valent_channel_close (ValentChannel  *channel,
                      GCancellable   *cancellable,
                      GError        **error)
{
  ValentChannelPrivate *priv = valent_channel_get_instance_private (channel);
  gboolean ret = TRUE;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_CHANNEL (channel), FALSE);
  g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  valent_object_lock (VALENT_OBJECT (channel));
  if (priv->base_stream != NULL && !g_io_stream_is_closed (priv->base_stream))
    {
      ret = g_io_stream_close (priv->base_stream, cancellable, error);
      g_queue_clear_full (&priv->output_buffer, valent_channel_write_cancel);
      g_clear_object (&priv->input_buffer);
    }
  valent_object_unlock (VALENT_OBJECT (channel));

  VALENT_RETURN (ret);
}

static void
valent_channel_close_task (GTask        *task,
                           gpointer      source_object,
                           gpointer      task_data,
                           GCancellable *cancellable)
{
  ValentChannel *self = source_object;
  GError *error = NULL;

  if (g_task_return_error_if_cancelled (task))
    return;

  if (valent_channel_close (self, cancellable, &error))
    g_task_return_boolean (task, TRUE);
  else
    g_task_return_error (task, error);
}

/**
 * valent_channel_close_async:
 * @channel: a #ValentChannel
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): a #GAsyncReadyCallback
 * @user_data: (closure): user supplied data
 *
 * Close the channel asynchronously.
 *
 * Call [method@Valent.Channel.close_finish] to get the result.
 *
 * Since: 1.0
 */
void
valent_channel_close_async (ValentChannel       *channel,
                            GCancellable        *cancellable,
                            GAsyncReadyCallback  callback,
                            gpointer             user_data)
{
  g_autoptr (GTask) task = NULL;

  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_CHANNEL (channel));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (channel, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_channel_close_async);
  g_task_run_in_thread (task, valent_channel_close_task);

  VALENT_EXIT;
}

/**
 * valent_channel_close_finish:
 * @channel: a #ValentChannel
 * @result: a #GAsyncResult
 * @error: (nullable): a #GError
 *
 * Finish an operation started by [method@Valent.Channel.close_async].
 *
 * Returns: %TRUE if successful, or %FALSE with @error set
 *
 * Since: 1.0
 */
gboolean
valent_channel_close_finish (ValentChannel  *channel,
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

static void
valent_channel_read_packet_task (GTask        *task,
                                 gpointer      source_object,
                                 gpointer      task_data,
                                 GCancellable *cancellable)
{
  ValentChannel *self = VALENT_CHANNEL (source_object);
  ValentChannelPrivate *priv = valent_channel_get_instance_private (self);
  g_autoptr (GDataInputStream) stream = NULL;
  g_autofree char *line = NULL;
  JsonNode *packet = NULL;
  GError *error = NULL;

  if (valent_channel_return_error_if_closed (self, task))
      return;

  stream = g_object_ref (priv->input_buffer);
  valent_object_unlock (VALENT_OBJECT (self));

  line = g_data_input_stream_read_line_utf8 (stream, NULL, cancellable, &error);

  if (error != NULL)
    return g_task_return_error (task, error);

  if (line == NULL)
    return g_task_return_new_error (task,
                                    G_IO_ERROR,
                                    G_IO_ERROR_CONNECTION_CLOSED,
                                    "Channel is closed");

  if ((packet = valent_packet_deserialize (line, &error)) == NULL)
    return g_task_return_error (task, error);

  g_task_return_pointer (task, packet, (GDestroyNotify)json_node_unref);
}

/**
 * valent_channel_read_packet:
 * @channel: a #ValentChannel
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): a #GAsyncReadyCallback
 * @user_data: (closure): user supplied data
 *
 * Read the next KDE Connect packet from @channel.
 *
 * Call [method@Valent.Channel.read_packet_finish] to get the result.
 *
 * Since: 1.0
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
 * Finish an operation started by [method@Valent.Channel.read_packet].
 *
 * Returns: (transfer full): a KDE Connect packet, or %NULL with @error set
 *
 * Since: 1.0
 */
JsonNode *
valent_channel_read_packet_finish (ValentChannel  *channel,
                                   GAsyncResult   *result,
                                   GError        **error)
{
  JsonNode *ret;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_CHANNEL (channel), NULL);
  g_return_val_if_fail (g_task_is_valid (result, channel), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  ret = g_task_propagate_pointer (G_TASK (result), error);

  VALENT_RETURN (ret);
}

static void
valent_channel_flush_task (GTask        *task,
                           gpointer      source_object,
                           gpointer      task_data,
                           GCancellable *cancellable)
{
  ValentChannel *self = VALENT_CHANNEL (source_object);
  ValentChannelPrivate *priv = valent_channel_get_instance_private (self);
  g_autoptr (GOutputStream) stream = NULL;

  if (valent_channel_return_error_if_closed (self, task))
      return;

  stream = g_object_ref (g_io_stream_get_output_stream (priv->base_stream));
  valent_object_unlock (VALENT_OBJECT (self));

  while (TRUE)
    {
      g_autoptr (GTask) next = NULL;
      JsonNode *packet = NULL;
      GCancellable *cancel = NULL;
      GError *error = NULL;

      /* Hold the lock to avoid dropping packets. */
      if (valent_channel_return_error_if_closed (self, task))
        return;

      next = g_queue_pop_head (&priv->output_buffer);
      priv->output_pending = (next != NULL);
      valent_object_unlock (VALENT_OBJECT (self));

      if (next == NULL)
        break;

      packet = g_task_get_task_data (next);
      cancel = g_task_get_cancellable (next);

      if (valent_packet_to_stream (stream, packet, cancel, &error))
        g_task_return_boolean (next, TRUE);
      else
        g_task_return_error (next, error);
    }

  g_task_return_boolean (task, TRUE);
}

/**
 * valent_channel_write_packet:
 * @channel: a #ValentChannel
 * @packet: a KDE Connect packet
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): a #GAsyncReadyCallback
 * @user_data: (closure): user supplied data
 *
 * Send a packet over the channel.
 *
 * Internally [class@Valent.Channel] uses an outgoing packet buffer, so
 * multiple requests can be started safely from any thread.
 *
 * Call [method@Valent.Channel.write_packet_finish] to get the result.
 *
 * Since: 1.0
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

  if (valent_channel_return_error_if_closed (channel, task))
    VALENT_EXIT;

  g_queue_push_tail (&priv->output_buffer, g_steal_pointer (&task));

  if (priv->output_pending == FALSE)
    {
      g_autoptr (GTask) operation = NULL;

      priv->output_pending = TRUE;

      operation = g_task_new (channel, NULL, NULL, NULL);
      g_task_set_source_tag (operation, valent_channel_flush_task);
      g_task_run_in_thread (operation, valent_channel_flush_task);
    }
  valent_object_unlock (VALENT_OBJECT (channel));

  VALENT_EXIT;
}

/**
 * valent_channel_write_packet_finish:
 * @channel: a #ValentChannel
 * @result: a #GAsyncResult
 * @error: (nullable): a #GError
 *
 * Finish an operation started by [method@Valent.Channel.write_packet].
 *
 * Returns: %TRUE if successful, or %FALSE with @error set
 *
 * Since: 1.0
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
 * valent_channel_store_data: (virtual store_data)
 * @channel: a #ValentChannel
 * @context: a #ValentContext
 *
 * Store channel metadata.
 *
 * This method is called to store channel specific data. Implementations can
 * override this method to store extra data (eg. TLS Certificate).
 *
 * Implementations that override [vfunc@Valent.Channel.store_data] must
 * chain-up.
 *
 * Since: 1.0
 */
void
valent_channel_store_data (ValentChannel *channel,
                           ValentContext  *context)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_CHANNEL (channel));
  g_return_if_fail (VALENT_IS_CONTEXT (context));

  VALENT_CHANNEL_GET_CLASS (channel)->store_data (channel, context);

  VALENT_EXIT;
}

/**
 * valent_channel_download: (virtual download)
 * @channel: a #ValentChannel
 * @packet: a KDE Connect packet
 * @cancellable: (nullable): a #GCancellable
 * @error: (nullable): a #GError
 *
 * Open an auxiliary connection, usually to download data.
 *
 * Implementations should use information from the `payloadTransferInfo` field
 * to open a connection and wait for it to be accepted. In most cases the remote
 * device will write data to the stream and then close it when finished.
 *
 * For example, a TCP-based implementation could connect to a port in the
 * `payloadTransferInfo` dictionary on the same host as the channel. When the
 * connection is accepted the caller can perform operations on it as required.
 *
 * Returns: (transfer full) (nullable): a #GIOStream
 *
 * Since: 1.0
 */
GIOStream *
valent_channel_download (ValentChannel  *channel,
                         JsonNode       *packet,
                         GCancellable   *cancellable,
                         GError        **error)
{
  GIOStream *ret;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_CHANNEL (channel), NULL);
  g_return_val_if_fail (VALENT_IS_PACKET (packet), NULL);
  g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  ret = VALENT_CHANNEL_GET_CLASS (channel)->download (channel,
                                                      packet,
                                                      cancellable,
                                                      error);

  VALENT_RETURN (ret);
}

/**
 * valent_channel_download_async: (virtual download_async)
 * @channel: a #ValentChannel
 * @packet: a KDE Connect packet
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): a #GAsyncReadyCallback
 * @user_data: (closure): user supplied data
 *
 * Open an auxiliary connection, usually to download data.
 *
 * This is a non-blocking variant of [method@Valent.Channel.download]. Call
 * [method@Valent.Channel.download_finish] to get the result.
 *
 * The default implementation of this method invokes
 * [vfunc@Valent.Channel.download] in a thread.
 *
 * Since: 1.0
 */
void
valent_channel_download_async (ValentChannel       *channel,
                               JsonNode            *packet,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_CHANNEL (channel));
  g_return_if_fail (VALENT_IS_PACKET (packet));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  VALENT_CHANNEL_GET_CLASS (channel)->download_async (channel,
                                                      packet,
                                                      cancellable,
                                                      callback,
                                                      user_data);

  VALENT_EXIT;
}

/**
 * valent_channel_download_finish: (virtual download_finish)
 * @channel: a #ValentChannel
 * @result: a #GAsyncResult
 * @error: (nullable): a #GError
 *
 * Finish an operation started with [method@Valent.Channel.download_async].
 *
 * Returns: (transfer full) (nullable): a #GIOStream
 *
 * Since: 1.0
 */
GIOStream *
valent_channel_download_finish (ValentChannel  *channel,
                                GAsyncResult   *result,
                                GError        **error)
{
  GIOStream *ret;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_CHANNEL (channel), NULL);
  g_return_val_if_fail (g_task_is_valid (result, channel), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  ret = VALENT_CHANNEL_GET_CLASS (channel)->download_finish (channel,
                                                             result,
                                                             error);

  VALENT_RETURN (ret);
}

/**
 * valent_channel_upload: (virtual upload)
 * @channel: a #ValentChannel
 * @packet: a KDE Connect packet
 * @cancellable: (nullable): a #GCancellable
 * @error: (nullable): a #GError
 *
 * Accept an auxiliary connection, usually to upload data.
 *
 * Implementations should set the `payloadTransferInfo` field with information
 * the peer can use to open a connection and wait to accept that connection. In
 * most cases the remote device with expect the caller to write to the stream
 * and then close it when finished.
 *
 * For example, a TCP-based implementation could start listening on a port then
 * send the packet with that port in the `payloadTransferInfo` dictionary. When
 * a connection is accepted the caller can perform operations on it as required.
 *
 * Returns: (transfer full) (nullable): a #GIOStream
 *
 * Since: 1.0
 */
GIOStream *
valent_channel_upload (ValentChannel  *channel,
                       JsonNode       *packet,
                       GCancellable   *cancellable,
                       GError        **error)
{
  GIOStream *ret;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_CHANNEL (channel), NULL);
  g_return_val_if_fail (VALENT_IS_PACKET (packet), NULL);
  g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  ret = VALENT_CHANNEL_GET_CLASS (channel)->upload (channel,
                                                    packet,
                                                    cancellable,
                                                    error);

  VALENT_RETURN (ret);
}

/**
 * valent_channel_upload_async: (virtual upload_async)
 * @channel: a #ValentChannel
 * @packet: a KDE Connect packet
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): a #GAsyncReadyCallback
 * @user_data: (closure): user supplied data
 *
 * Accept an auxiliary connection, usually to upload data.
 *
 * This is a non-blocking variant of [method@Valent.Channel.upload]. Call
 * [method@Valent.Channel.upload_finish] to get the result.
 *
 * The default implementation of this method invokes
 * [vfunc@Valent.Channel.upload] in a thread.
 *
 * Since: 1.0
 */
void
valent_channel_upload_async (ValentChannel       *channel,
                             JsonNode            *packet,
                             GCancellable        *cancellable,
                             GAsyncReadyCallback  callback,
                             gpointer             user_data)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_CHANNEL (channel));
  g_return_if_fail (VALENT_IS_PACKET (packet));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  VALENT_CHANNEL_GET_CLASS (channel)->upload_async (channel,
                                                    packet,
                                                    cancellable,
                                                    callback,
                                                    user_data);

  VALENT_EXIT;
}

/**
 * valent_channel_upload_finish: (virtual upload_finish)
 * @channel: a #ValentChannel
 * @result: a #GAsyncResult
 * @error: (nullable): a #GError
 *
 * Finish an operation started with [method@Valent.Channel.upload_async].
 *
 * Returns: (transfer full) (nullable): a #GIOStream
 *
 * Since: 1.0
 */
GIOStream *
valent_channel_upload_finish (ValentChannel  *channel,
                              GAsyncResult   *result,
                              GError        **error)
{
  GIOStream *ret;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_CHANNEL (channel), NULL);
  g_return_val_if_fail (g_task_is_valid (result, channel), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  ret = VALENT_CHANNEL_GET_CLASS (channel)->upload_finish (channel,
                                                           result,
                                                           error);

  VALENT_RETURN (ret);
}

