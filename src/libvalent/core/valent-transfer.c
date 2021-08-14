// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-transfer"

#include "config.h"

#include "valent-core-enums.h"

#include "valent-channel.h"
#include "valent-device.h"
#include "valent-macros.h"
#include "valent-packet.h"
#include "valent-transfer.h"


/**
 * SECTION:valenttransfer
 * @short_description: An object representing a remote transfer
 * @title: ValentTransfer
 * @stability: Unstable
 * @include: libvalent-core.h
 *
 * The #ValentTransfer object represents a data transfer to or from a device.
 */

typedef struct
{
  ValentDevice        *device;
  GCancellable        *cancellable;
  GPtrArray           *items;

  /* Transfer Properties */
  char                *id;
  ValentTransferState  state;
} ValentTransferPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (ValentTransfer, valent_transfer, G_TYPE_OBJECT)


enum {
  PROP_0,
  PROP_DEVICE,
  PROP_ID,
  PROP_STATE,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };


/*
 * Transfer Item
 */
typedef struct
{
  JsonNode      *packet;
  GFile         *file;
  GBytes        *bytes;
  GInputStream  *source;
  GOutputStream *target;
  gssize         size;
} TransferItem;

static void
transfer_item_free (gpointer data)
{
  TransferItem *item = data;

  g_clear_pointer (&item->packet, json_node_unref);
  g_clear_object (&item->file);
  g_clear_pointer (&item->bytes, g_bytes_unref);
  g_clear_object (&item->source);
  g_clear_object (&item->target);
  g_free (item);
}

static gpointer
stream_from_bytes (GBytes        *bytes,
                   gssize        *size,
                   GCancellable  *cancellable,
                   GError       **error)
{
  gpointer out = NULL;

  return out;
}

/**
 * transfer_item_prepare:
 * @item: a #TransferItem
 * @cancellable: (nullable): a #GCancellable
 * @error: (nullable): a #GError
 *
 * Execute the transfer described by @item.
 *
 * Returns: %TRUE, or %FAALSE with @error set
 */
static gboolean
transfer_item_prepare (TransferItem  *item,
                       GCancellable  *cancellable,
                       GError       **error)
{
  if (valent_packet_has_payload (item->packet))
    {
      if (G_IS_OUTPUT_STREAM (item->target))
        return TRUE;

      if (item->file != NULL)
        {
          GFileOutputStream *stream;

          stream = g_file_replace (item->file,
                                   NULL,
                                   FALSE,
                                   G_FILE_CREATE_REPLACE_DESTINATION,
                                   cancellable,
                                   error);

          if (stream != NULL)
            item->target = G_OUTPUT_STREAM (stream);
        }
      else if (item->bytes != NULL)
        item->target = stream_from_bytes (item->bytes, NULL, cancellable, error);

      return G_IS_OUTPUT_STREAM (item->target);
    }
  else
    {
      if (G_IS_INPUT_STREAM (item->source))
        return TRUE;

      if (item->file != NULL)
        {
          g_autoptr (GFileInfo) info = NULL;
          GFileInputStream *stream;

          info = g_file_query_info (item->file,
                                    G_FILE_ATTRIBUTE_STANDARD_SIZE,
                                    G_FILE_QUERY_INFO_NONE,
                                    cancellable,
                                    error);

          if (info == NULL)
            return FALSE;

          stream = g_file_read (item->file, cancellable, error);

          if (stream != NULL)
            {
              item->size = g_file_info_get_size (info);
              item->source = G_INPUT_STREAM (stream);
            }
        }
      else if (item->bytes != NULL)
        {
          item->source = g_memory_input_stream_new_from_bytes (item->bytes);

          if (item->source != NULL)
            item->size = g_bytes_get_size (item->bytes);
        }

      return G_IS_INPUT_STREAM (item->source);
    }
}

/**
 * transfer_item_execute:
 * @item: a #TransferItem
 * @channel: a #ValentChannel
 * @cancellable: (nullable): a #GCancellable
 * @error: (nullable): a #GError
 *
 * Execute the transfer described by @item.
 *
 * Returns: %TRUE, or %FAALSE with @error set
 */
static gboolean
transfer_item_execute (TransferItem   *item,
                       ValentChannel  *channel,
                       GCancellable   *cancellable,
                       GError        **error)
{
  g_autoptr (GIOStream) stream = NULL;
  GInputStream *source;
  GOutputStream *target;
  gssize transferred;

  if (!transfer_item_prepare (item, cancellable, error))
    return FALSE;

  if (valent_packet_has_payload (item->packet))
    {
      item->size = valent_packet_get_payload_size (item->packet);
      stream = valent_channel_download (channel,
                                        item->packet,
                                        cancellable,
                                        error);

      if (stream == NULL)
        return FALSE;

      source = g_io_stream_get_input_stream (stream);
      target = item->target;
    }
  else
    {
      valent_packet_set_payload_size (item->packet, item->size);
      stream = valent_channel_upload (channel,
                                      item->packet,
                                      cancellable,
                                      error);

      if (stream == NULL)
        return FALSE;

      source = item->source;
      target = g_io_stream_get_output_stream (stream);
    }

  transferred = g_output_stream_splice (target,
                                        source,
                                        (G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE |
                                         G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET),
                                        cancellable,
                                        error);

  return (item->size == transferred);
}

static void
execute_task (GTask        *task,
              gpointer      source_object,
              gpointer      user_data,
              GCancellable *cancellable)
{
  ValentTransfer *self = source_object;
  ValentTransferPrivate *priv = valent_transfer_get_instance_private (self);
  GError *error = NULL;

  for (unsigned int i = 0; i < priv->items->len; i++)
    {
      TransferItem *item = g_ptr_array_index (priv->items, i);
      ValentChannel *channel;

      if (g_task_return_error_if_cancelled (task))
        return;

      /* If the device has no channel, that means it's disconnected and we can't
       * tell it we're ready to download or upload via the packet channel */
      channel = valent_device_get_channel (priv->device);

      if (channel == NULL)
        return g_task_return_new_error (task,
                                        G_IO_ERROR,
                                        G_IO_ERROR_CONNECTION_CLOSED,
                                        "Device is disconnected");

      if (!transfer_item_execute (item, channel, cancellable, &error))
        return g_task_return_error (task, error);
    }

  return g_task_return_boolean (task, TRUE);
}

/*
 * GObject
 */
static void
valent_transfer_dispose (GObject *object)
{
  ValentTransfer *self = VALENT_TRANSFER (object);
  ValentTransferPrivate *priv = valent_transfer_get_instance_private (self);

  g_cancellable_cancel (priv->cancellable);

  G_OBJECT_CLASS (valent_transfer_parent_class)->dispose (object);
}

static void
valent_transfer_finalize (GObject *object)
{
  ValentTransfer *self = VALENT_TRANSFER (object);
  ValentTransferPrivate *priv = valent_transfer_get_instance_private (self);

  g_clear_object (&priv->device);
  g_clear_object (&priv->cancellable);
  g_clear_pointer (&priv->id, g_free);
  g_clear_pointer (&priv->items, g_ptr_array_unref);

  G_OBJECT_CLASS (valent_transfer_parent_class)->finalize (object);
}

static void
valent_transfer_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  ValentTransfer *self = VALENT_TRANSFER (object);
  ValentTransferPrivate *priv = valent_transfer_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_DEVICE:
      g_value_set_object (value, priv->device);
      break;

    case PROP_ID:
      g_value_set_string (value, valent_transfer_get_id (self));
      break;

    case PROP_STATE:
      g_value_set_enum (value, priv->state);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_transfer_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  ValentTransfer *self = VALENT_TRANSFER (object);
  ValentTransferPrivate *priv = valent_transfer_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_DEVICE:
      priv->device = g_value_dup_object (value);
      break;

    case PROP_ID:
      valent_transfer_set_id (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_transfer_class_init (ValentTransferClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = valent_transfer_dispose;
  object_class->finalize = valent_transfer_finalize;
  object_class->get_property = valent_transfer_get_property;
  object_class->set_property = valent_transfer_set_property;

  /**
   * ValentTransfer:device:
   *
   * The #ValentDevice this channel is attached to.
   */
  properties [PROP_DEVICE] =
    g_param_spec_object ("device",
                         "Device",
                         "Device",
                         VALENT_TYPE_DEVICE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentTransfer:id:
   *
   * A unique identifier for the transfer. A random UUID will be generated on-
   * demand if necessary.
   *
   * Although not enforced, this should be set to a standard UUIDv4 string.
   */
  properties [PROP_ID] =
    g_param_spec_string ("id",
                         "ID",
                         "Unique identifier for the channel",
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentTransfer:state:
   *
   * Whether the transfer has completed.
   */
  properties [PROP_STATE] =
    g_param_spec_enum ("state",
                       "State",
                       "The transfer state",
                       VALENT_TYPE_TRANSFER_STATE,
                       VALENT_TRANSFER_STATE_NONE,
                       (G_PARAM_READABLE |
                        G_PARAM_EXPLICIT_NOTIFY |
                        G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
valent_transfer_init (ValentTransfer *self)
{
  ValentTransferPrivate *priv = valent_transfer_get_instance_private (self);

  priv->cancellable = g_cancellable_new ();
  priv->items = g_ptr_array_new_with_free_func (transfer_item_free);
  priv->state = VALENT_TRANSFER_STATE_NONE;
}

/**
 * valent_transfer_new:
 * @device: a #ValentDevice
 *
 * Create a new transfer object for @device.
 *
 * Returns: (transfer full): a #ValentTransfer
 */
ValentTransfer *
valent_transfer_new (ValentDevice *device)
{
  g_return_val_if_fail (VALENT_IS_DEVICE (device), NULL);

  return g_object_new (VALENT_TYPE_TRANSFER,
                       "device", device,
                       NULL);
}

/**
 * valent_transfer_add_bytes:
 * @transfer: a #ValentTransfer
 * @packet: a #JsonNode
 * @bytes: a #GBytes
 *
 * Add @file to the transfer queue.
 */
void
valent_transfer_add_bytes (ValentTransfer *transfer,
                           JsonNode       *packet,
                           GBytes         *bytes)
{
  ValentTransferPrivate *priv = valent_transfer_get_instance_private (transfer);
  TransferItem *item;

  g_return_if_fail (VALENT_IS_TRANSFER (transfer));
  g_return_if_fail (VALENT_IS_PACKET (packet));
  g_return_if_fail (bytes != NULL);

  item = g_new0 (TransferItem, 1);
  item->packet = json_node_ref (packet);
  item->bytes = g_bytes_ref (bytes);

  g_ptr_array_add (priv->items, item);
}

/**
 * valent_transfer_add_file:
 * @transfer: a #ValentTransfer
 * @packet: a #JsonNode
 * @file: a #GFile
 *
 * Add @file to the transfer queue.
 */
void
valent_transfer_add_file (ValentTransfer *transfer,
                          JsonNode       *packet,
                          GFile          *file)
{
  ValentTransferPrivate *priv = valent_transfer_get_instance_private (transfer);
  TransferItem *item;

  g_return_if_fail (VALENT_IS_TRANSFER (transfer));
  g_return_if_fail (VALENT_IS_PACKET (packet));
  g_return_if_fail (G_IS_FILE (file));

  item = g_new0 (TransferItem, 1);
  item->packet = json_node_ref (packet);
  item->file = g_object_ref (file);

  g_ptr_array_add (priv->items, item);
}

/**
 * valent_transfer_add_stream:
 * @transfer: a #ValentTransfer
 * @packet: a #JsonNode
 * @source: (nullable): a #GInputStream
 * @target: (nullable): a #GOutputStream
 * @size: a payload size
 *
 * Add a stream to the transfer queue.
 *
 * If @packet describes an upload, @source and @size must be given.
 *
 * If @packet describes a download, @target must be given an @size is ignored.
 */
void
valent_transfer_add_stream (ValentTransfer *transfer,
                            JsonNode       *packet,
                            GInputStream   *source,
                            GOutputStream  *target,
                            gssize          size)
{
  ValentTransferPrivate *priv = valent_transfer_get_instance_private (transfer);
  TransferItem *item;

  g_return_if_fail (VALENT_IS_TRANSFER (transfer));
  g_return_if_fail (VALENT_IS_PACKET (packet));
  g_return_if_fail (G_IS_INPUT_STREAM (source) || G_IS_OUTPUT_STREAM (target));

  if (source != NULL && !size)
    {
      g_warning ("Source stream set without payload size");
      return;
    }

  item = g_new0 (TransferItem, 1);
  item->packet = json_node_ref (packet);

  if (source != NULL)
    {
      item->source = g_object_ref (source);
      item->size = size;
    }
  else
    item->target = g_object_ref (target);

  g_ptr_array_add (priv->items, item);
}

/**
 * valent_transfer_cache_file:
 * @transfer: a #ValentTransfer
 * @packet: a #JsonNode
 * @name: a filename or other unique string
 *
 * Download and store the payload described by @packet in the device cache,
 * using @name to generate hash string.
 *
 * Returns: (transfer full): a #GFile
 */
GFile *
valent_transfer_cache_file (ValentTransfer *transfer,
                            JsonNode       *packet,
                            const char     *name)
{
  ValentTransferPrivate *priv = valent_transfer_get_instance_private (transfer);
  ValentData *data;
  g_autofree char *hash = NULL;
  TransferItem *item;

  g_return_val_if_fail (VALENT_IS_TRANSFER (transfer), NULL);
  g_return_val_if_fail (VALENT_IS_PACKET (packet), NULL);
  g_return_val_if_fail (name != NULL, NULL);
  g_return_val_if_fail (valent_packet_has_payload (packet), NULL);

  /* Hash the given name to ensure it's a safe filename */
  data = valent_device_get_data (priv->device);
  hash = g_compute_checksum_for_string (G_CHECKSUM_MD5, name, -1);

  item = g_new0 (TransferItem, 1);
  item->packet = json_node_ref (packet);
  item->file = valent_data_new_cache_file (data, hash);
  g_ptr_array_add (priv->items, item);

  return g_object_ref (item->file);
}

/**
 * valent_transfer_cancel:
 * @transfer: a #ValentTransfer
 *
 * Cancel the transfer if in progress.
 */
void
valent_transfer_cancel (ValentTransfer *transfer)
{
  ValentTransferPrivate *priv = valent_transfer_get_instance_private (transfer);

  g_return_if_fail (VALENT_IS_TRANSFER (transfer));

  g_cancellable_cancel (priv->cancellable);

  g_clear_object (&priv->cancellable);
  priv->cancellable = g_cancellable_new ();
}

/**
 * valent_transfer_get_device:
 * @transfer: a #ValentTransfer
 *
 * Get the #ValentDevice for @transfer.
 *
 * Returns: (transfer none): a #ValentDevice
 */
ValentDevice *
valent_transfer_get_device (ValentTransfer *transfer)
{
  ValentTransferPrivate *priv = valent_transfer_get_instance_private (transfer);

  g_return_val_if_fail (VALENT_IS_TRANSFER (transfer), NULL);

  return priv->device;
}

/**
 * valent_transfer_get_id:
 * @transfer: a #ValentTransfer
 *
 * Get the unique ID for @transfer.
 *
 * Returns: a unique id
 */
const char *
valent_transfer_get_id (ValentTransfer *transfer)
{
  ValentTransferPrivate *priv = valent_transfer_get_instance_private (transfer);

  g_return_val_if_fail (VALENT_IS_TRANSFER (transfer), NULL);

  if (priv->id == NULL)
    priv->id = g_uuid_string_random ();

  return priv->id;
}

/**
 * valent_transfer_set_id:
 * @transfer: a #ValentTransfer
 * @id: a unique id
 *
 * Set the id for @transfer to @id.
 */
void
valent_transfer_set_id (ValentTransfer *transfer,
                        const char     *id)
{
  ValentTransferPrivate *priv = valent_transfer_get_instance_private (transfer);

  g_return_if_fail (VALENT_IS_TRANSFER (transfer));
  g_return_if_fail (id != NULL);

  g_clear_pointer (&priv->id, g_free);
  priv->id = g_strdup (id);
}

/**
 * valent_transfer_execute:
 * @transfer: a #ValentTransfer
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): a #GAsyncReadyCallback
 * @user_data: (closure): user supplied data
 *
 * Start the transfer.
 */
void
valent_transfer_execute (ValentTransfer      *transfer,
                         GCancellable        *cancellable,
                         GAsyncReadyCallback  callback,
                         gpointer             user_data)
{
  ValentTransferPrivate *priv = valent_transfer_get_instance_private (transfer);
  g_autoptr (GTask) task = NULL;

  g_return_if_fail (VALENT_IS_TRANSFER (transfer));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  if (cancellable != NULL)
    g_signal_connect_object (cancellable,
                             "cancelled",
                             G_CALLBACK (g_cancellable_cancel),
                             priv->cancellable,
                             G_CONNECT_SWAPPED);

  task = g_task_new (transfer, priv->cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_transfer_execute);
  g_task_run_in_thread (task, execute_task);
}

/**
 * valent_transfer_execute_finish:
 * @transfer: a #ValentTransfer
 * @result: a #GAsyncResult
 * @error: (nullable): a #GError
 *
 * Finish a transfer started with valent_transfer_execute().
 *
 * Returns: %TRUE on success or %FALSE with @error set.
 */
gboolean
valent_transfer_execute_finish (ValentTransfer  *transfer,
                                GAsyncResult    *result,
                                GError         **error)
{
  g_return_val_if_fail (g_task_is_valid (result, transfer), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

