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
#include "valent-object.h"
#include "valent-packet.h"

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
  GIOStream         *base_stream;
  JsonNode          *identity;
  JsonNode          *peer_identity;

  /* Packet Buffer */
  GDataInputStream  *input_buffer;
  GQueue             output_buffer;
  unsigned int       output_loop : 1;
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
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_CONNECTION_CLOSED,
                               "Channel is closed");
      valent_object_unlock (VALENT_OBJECT (self));
      return TRUE;
    }

  return FALSE;
}

static void
valent_channel_set_base_stream (ValentChannel *self,
                                GIOStream     *base_stream)
{
  ValentChannelPrivate *priv = valent_channel_get_instance_private (self);
  GInputStream *input_stream;

  g_assert (VALENT_IS_CHANNEL (self));
  g_assert (G_IS_IO_STREAM (base_stream));

  valent_object_lock (VALENT_OBJECT (self));
  priv->base_stream = g_object_ref (base_stream);
  input_stream = g_io_stream_get_input_stream (base_stream);
  priv->input_buffer = g_object_new (G_TYPE_DATA_INPUT_STREAM,
                                     "base-stream",       input_stream,
                                     "close-base-stream", FALSE,
                                     NULL);
  valent_object_unlock (VALENT_OBJECT (self));
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

  g_clear_object (&priv->input_buffer);
  g_queue_clear_full (&priv->output_buffer, valent_channel_write_cancel);

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

  g_queue_init (&priv->output_buffer);
}

/**
 * valent_channel_ref_base_stream:
 * @channel: a #ValentChannel
 *
 * Gets the #GIOStream for the channel, or %NULL if unset.
 *
 * Returns: (transfer full) (nullable): the base #GIOStream.
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
  gboolean ret = TRUE;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_CHANNEL (channel), FALSE);
  g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  valent_object_lock (VALENT_OBJECT (channel));
  if (priv->base_stream != NULL)
    ret = g_io_stream_close (priv->base_stream, cancellable, error);
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
  ValentChannelPrivate *priv = valent_channel_get_instance_private (self);
  GError *error = NULL;

  if (valent_channel_return_error_if_closed (self, task))
      return;

  if (!g_io_stream_close (priv->base_stream, cancellable, &error))
    g_task_return_error (task, error);
  else
    g_task_return_boolean (task, TRUE);

  valent_object_unlock (VALENT_OBJECT (self));
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

  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_CHANNEL (channel));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (channel, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_channel_close_async);

  valent_object_lock (VALENT_OBJECT (channel));

  if (priv->base_stream == NULL || g_io_stream_is_closed (priv->base_stream))
    g_task_return_boolean (task, TRUE);
  else
    g_task_run_in_thread (task, valent_channel_close_task);

  valent_object_unlock (VALENT_OBJECT (channel));

  VALENT_EXIT;
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

static void
valent_channel_write_loop (GTask        *task,
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
      GError *error = NULL;

      /* Lock while deciding whether to exit the loop, otherwise packets queued
       * in the interim may not get sent. */
      valent_object_lock (VALENT_OBJECT (self));
      next = g_queue_pop_head (&priv->output_buffer);
      priv->output_loop = (next != NULL);
      valent_object_unlock (VALENT_OBJECT (self));

      if (next == NULL)
        break;

      packet = g_task_get_task_data (next);

      if (!valent_packet_to_stream (stream, packet, cancellable, &error))
        {
          g_task_return_error (next, error);
          break;
        }

      g_task_return_boolean (next, TRUE);
    }

  g_task_return_boolean (task, TRUE);
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

  if (valent_channel_return_error_if_closed (channel, task))
    return;

  g_queue_push_tail (&priv->output_buffer, g_steal_pointer (&task));

  if (priv->output_loop == FALSE)
    {
      g_autoptr (GTask) loop = NULL;
      g_autoptr (GCancellable) close = NULL;

      priv->output_loop = TRUE;

      close = valent_object_ref_cancellable (VALENT_OBJECT (channel));
      loop = g_task_new (channel, close, NULL, NULL);
      g_task_set_source_tag (loop, valent_channel_write_loop);
      g_task_run_in_thread (loop, valent_channel_write_loop);
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
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_CHANNEL (channel));
  g_return_if_fail (VALENT_IS_DATA (data));

  VALENT_CHANNEL_GET_CLASS (channel)->store_data (channel, data);

  VALENT_EXIT;
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

