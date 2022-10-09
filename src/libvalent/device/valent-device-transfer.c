// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-device-transfer"

#include "config.h"

#include <math.h>

#include <libvalent-core.h>

#include "valent-channel.h"
#include "valent-device.h"
#include "valent-device-transfer.h"
#include "valent-packet.h"


/**
 * ValentDeviceTransfer:
 *
 * A class for device file transfers.
 *
 * #ValentDeviceTransfer is an implementation of [class@Valent.Transfer] for the
 * common case of transferring a file between devices.
 *
 * The direction of the transfer is automatically detected from the content of
 * [property@Valent.DeviceTransfer:packet]. If the KDE Connect packet holds
 * payload information the transfer is assumed to be a download, otherwise it is
 * assumed to be an upload.
 *
 * Since: 1.0
 */

struct _ValentDeviceTransfer
{
  ValentObject  parent_instance;

  ValentDevice *device;
  GFile        *file;
  JsonNode     *packet;
};

G_DEFINE_TYPE (ValentDeviceTransfer, valent_device_transfer, VALENT_TYPE_TRANSFER)


enum {
  PROP_0,
  PROP_DEVICE,
  PROP_FILE,
  PROP_PACKET,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };


static inline void
valent_device_transfer_update_packet (JsonNode  *packet,
                                      GFileInfo *info)
{
  JsonObject *body;
  guint64 btime_s, mtime_s;
  guint32 btime_us, mtime_us;
  guint64 creation_time;
  guint64 last_modified;
  goffset payload_size;

  g_assert (VALENT_IS_PACKET (packet));

  btime_s = g_file_info_get_attribute_uint64 (info, "time::created");
  btime_us = g_file_info_get_attribute_uint32 (info, "time::created-usec");
  creation_time = (btime_s * 1000) + floor (btime_us / 1000);

  mtime_s = g_file_info_get_attribute_uint64 (info, "time::created");
  mtime_us = g_file_info_get_attribute_uint32 (info, "time::created-usec");
  last_modified = (mtime_s * 1000) + floor (mtime_us / 1000);

  payload_size = g_file_info_get_size (info);

  body = valent_packet_get_body (packet);
  json_object_set_int_member (body, "creationTime", creation_time);
  json_object_set_int_member (body, "lastModified", last_modified);
  valent_packet_set_payload_size (packet, payload_size);
}

/*
 * ValentDeviceTransfer
 */
static void
valent_device_transfer_execute_task (GTask        *task,
                                     gpointer      source_object,
                                     gpointer      task_data,
                                     GCancellable *cancellable)
{
  ValentDeviceTransfer *self = VALENT_DEVICE_TRANSFER (source_object);
  g_autoptr (ValentChannel) channel = NULL;
  g_autoptr (GFile) file = NULL;
  g_autoptr (JsonNode) packet = NULL;
  g_autoptr (GIOStream) stream = NULL;
  g_autoptr (GInputStream) source = NULL;
  g_autoptr (GOutputStream) target = NULL;
  gboolean is_download = FALSE;
  gssize transferred;
  gint64 last_modified = 0;
  gint64 creation_time = 0;
  goffset payload_size;
  GError *error = NULL;

  if (g_task_return_error_if_cancelled (task))
    return;

  valent_object_lock (VALENT_OBJECT (self));
  channel = valent_device_ref_channel (self->device);
  file = g_object_ref (self->file);
  packet = json_node_ref (self->packet);
  valent_object_unlock (VALENT_OBJECT (self));

  if (channel == NULL)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_CONNECTED,
                               "Device is disconnected");
      return;
    }

  /* Determine if this is a download or an upload. This should be reliable,
   * given that the channel service must set the `payloadTransferInfo` field in
   * its valent_channel_upload() implementation. */
  is_download = valent_packet_has_payload (packet);

  if (is_download)
    {
      target = (GOutputStream *)g_file_replace (file,
                                                NULL,
                                                FALSE,
                                                G_FILE_CREATE_REPLACE_DESTINATION,
                                                cancellable,
                                                &error);

      if (target == NULL)
        return g_task_return_error (task, error);

      stream = valent_channel_download (channel, packet, cancellable, &error);

      if (stream == NULL)
        return g_task_return_error (task, error);

      source = g_object_ref (g_io_stream_get_input_stream (stream));
    }
  else
    {
      g_autoptr (GFileInfo) info = NULL;

      info = g_file_query_info (file,
                                G_FILE_ATTRIBUTE_TIME_CREATED","
                                G_FILE_ATTRIBUTE_TIME_CREATED_USEC","
                                G_FILE_ATTRIBUTE_TIME_MODIFIED","
                                G_FILE_ATTRIBUTE_TIME_MODIFIED_USEC","
                                G_FILE_ATTRIBUTE_STANDARD_SIZE,
                                G_FILE_QUERY_INFO_NONE,
                                cancellable,
                                &error);

      if (info == NULL)
        return g_task_return_error (task, error);

      source = (GInputStream *)g_file_read (file, cancellable, &error);

      if (source == NULL)
        return g_task_return_error (task, error);

      valent_device_transfer_update_packet (packet, info);
      stream = valent_channel_upload (channel, packet, cancellable, &error);

      if (stream == NULL)
        return g_task_return_error (task, error);

      target = g_object_ref (g_io_stream_get_output_stream (stream));
    }

  /* Transfer the payload */
  transferred = g_output_stream_splice (target,
                                        source,
                                        (G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE |
                                         G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET),
                                        cancellable,
                                        &error);

  if (error != NULL)
    {
      if (is_download)
        g_file_delete (file, NULL, NULL);

      return g_task_return_error (task, error);
    }

  /* If possible, confirm the transferred size with the payload size */
  payload_size = valent_packet_get_payload_size (packet);

  if G_UNLIKELY (payload_size > G_MAXSSIZE)
    {
      g_warning ("%s(): Payload size greater than %"G_GSSIZE_FORMAT";"
                 "unable to confirm transfer completion",
                 G_STRFUNC, G_MAXSSIZE);
    }
  else if (transferred < payload_size)
    {
      g_debug ("%s(): Transfer incomplete (%"G_GSSIZE_FORMAT"/%"G_GOFFSET_FORMAT" bytes)",
               G_STRFUNC, transferred, payload_size);

      if (is_download)
        g_file_delete (file, NULL, NULL);

      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_PARTIAL_INPUT,
                               "Transfer incomplete");
      return;
    }

  /* Attempt to set file attributes for downloaded files. */
  if (is_download)
    {
      /* NOTE: this is not supported by the Linux kernel... */
      if (valent_packet_get_int (packet, "creationTime", &creation_time))
        {
          gboolean success;
          g_autoptr (GError) warn = NULL;

          success = g_file_set_attribute_uint64 (file,
                                                 G_FILE_ATTRIBUTE_TIME_CREATED,
                                                 floor (creation_time / 1000),
                                                 G_FILE_QUERY_INFO_NONE,
                                                 cancellable,
                                                 &warn);

          if (success)
            {
              g_file_set_attribute_uint32 (file,
                                           G_FILE_ATTRIBUTE_TIME_CREATED_USEC,
                                           (creation_time % 1000) * 1000,
                                           G_FILE_QUERY_INFO_NONE,
                                           cancellable,
                                           &warn);
            }

          if (warn != NULL)
            g_debug ("%s: %s", G_OBJECT_TYPE_NAME (self), warn->message);
        }

      if (valent_packet_get_int (packet, "lastModified", &last_modified))
        {
          gboolean success;
          g_autoptr (GError) warn = NULL;

          success = g_file_set_attribute_uint64 (file,
                                                 G_FILE_ATTRIBUTE_TIME_MODIFIED,
                                                 floor (last_modified / 1000),
                                                 G_FILE_QUERY_INFO_NONE,
                                                 cancellable,
                                                 &warn);

          if (success)
            {
              g_file_set_attribute_uint32 (file,
                                           G_FILE_ATTRIBUTE_TIME_MODIFIED_USEC,
                                           (last_modified % 1000) * 1000,
                                           G_FILE_QUERY_INFO_NONE,
                                           cancellable,
                                           &warn);
            }

          if (warn != NULL)
            g_debug ("%s: %s", G_OBJECT_TYPE_NAME (self), warn->message);
        }
    }

  g_task_return_boolean (task, TRUE);
}

static void
valent_device_transfer_execute (ValentTransfer      *transfer,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  g_autoptr (GTask) task = NULL;

  VALENT_ENTRY;

  g_assert (VALENT_IS_DEVICE_TRANSFER (transfer));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (transfer, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_device_transfer_execute);
  g_task_run_in_thread (task, valent_device_transfer_execute_task);

  VALENT_EXIT;
}

/*
 * GObject
 */
static void
valent_device_transfer_finalize (GObject *object)
{
  ValentDeviceTransfer *self = VALENT_DEVICE_TRANSFER (object);

  valent_object_lock (VALENT_OBJECT (self));
  g_clear_object (&self->device);
  g_clear_object (&self->file);
  g_clear_pointer (&self->packet, json_node_unref);
  valent_object_unlock (VALENT_OBJECT (self));

  G_OBJECT_CLASS (valent_device_transfer_parent_class)->finalize (object);
}

static void
valent_device_transfer_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  ValentDeviceTransfer *self = VALENT_DEVICE_TRANSFER (object);

  switch (prop_id)
    {
    case PROP_DEVICE:
      g_value_take_object (value, valent_device_transfer_ref_device (self));
      break;

    case PROP_FILE:
      g_value_take_object (value, valent_device_transfer_ref_file (self));
      break;

    case PROP_PACKET:
      g_value_take_boxed (value, valent_device_transfer_ref_packet (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_device_transfer_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  ValentDeviceTransfer *self = VALENT_DEVICE_TRANSFER (object);

  switch (prop_id)
    {
    case PROP_DEVICE:
      valent_object_lock (VALENT_OBJECT (self));
      self->device = g_value_dup_object (value);
      valent_object_unlock (VALENT_OBJECT (self));
      break;

    case PROP_FILE:
      valent_object_lock (VALENT_OBJECT (self));
      self->file = g_value_dup_object (value);
      valent_object_unlock (VALENT_OBJECT (self));
      break;

    case PROP_PACKET:
      valent_object_lock (VALENT_OBJECT (self));
      self->packet = g_value_dup_boxed (value);
      valent_object_unlock (VALENT_OBJECT (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_device_transfer_class_init (ValentDeviceTransferClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentTransferClass *transfer_class = VALENT_TRANSFER_CLASS (klass);

  object_class->finalize = valent_device_transfer_finalize;
  object_class->get_property = valent_device_transfer_get_property;
  object_class->set_property = valent_device_transfer_set_property;

  transfer_class->execute = valent_device_transfer_execute;

  /**
   * ValentDeviceTransfer:device: (getter ref_device)
   *
   * The [class@Valent.Device] this transfer is for.
   *
   * Since: 1.0
   */
  properties [PROP_DEVICE] =
    g_param_spec_object ("device", NULL, NULL,
                         VALENT_TYPE_DEVICE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentDeviceTransfer:file: (getter ref_file)
   *
   * The local [iface@Gio.File].
   *
   * If this a download, then this is the destination. If it's upload, this is
   * the source.
   *
   * Since: 1.0
   */
  properties [PROP_FILE] =
    g_param_spec_object ("file", NULL, NULL,
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentDeviceTransfer:packet: (getter ref_packet)
   *
   * The KDE Connect packet describing the payload.
   *
   * Since: 1.0
   */
  properties [PROP_PACKET] =
    g_param_spec_boxed ("packet", NULL, NULL,
                        JSON_TYPE_NODE,
                        (G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
valent_device_transfer_init (ValentDeviceTransfer *self)
{
}

/**
 * valent_device_transfer_new_for_file:
 * @device: a #ValentDevice
 * @packet: a KDE Connect packet
 * @file: a #GFile
 *
 * A convenience for creating a simple file transfer.
 *
 * Returns: (transfer full) (nullable): a #ValentDeviceTransfer
 *
 * Since: 1.0
 */
ValentTransfer *
valent_device_transfer_new_for_file (ValentDevice *device,
                                     JsonNode     *packet,
                                     GFile        *file)
{
  g_return_val_if_fail (VALENT_IS_DEVICE (device), NULL);
  g_return_val_if_fail (VALENT_IS_PACKET (packet), NULL);
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  return g_object_new (VALENT_TYPE_DEVICE_TRANSFER,
                       "device", device,
                       "packet", packet,
                       "file",   file,
                       NULL);
}

/**
 * valent_device_transfer_ref_device: (get-property device)
 * @transfer: a #ValentDeviceTransfer
 *
 * Get the [class@Valent.Device].
 *
 * Returns: (transfer full) (nullable): a #ValentDevice
 *
 * Since: 1.0
 */
ValentDevice *
valent_device_transfer_ref_device (ValentDeviceTransfer *transfer)
{
  g_autoptr (ValentDevice) ret = NULL;

  g_return_val_if_fail (VALENT_IS_DEVICE_TRANSFER (transfer), NULL);

  valent_object_lock (VALENT_OBJECT (transfer));
  if (transfer->device != NULL)
    ret = g_object_ref (transfer->device);
  valent_object_unlock (VALENT_OBJECT (transfer));

  return g_steal_pointer (&ret);
}

/**
 * valent_device_transfer_ref_file: (get-property file)
 * @transfer: a #ValentDeviceTransfer
 *
 * Get the local [iface@Gio.File].
 *
 * Returns: (transfer full) (nullable): a #GFile
 *
 * Since: 1.0
 */
GFile *
valent_device_transfer_ref_file (ValentDeviceTransfer *transfer)
{
  g_autoptr (GFile) ret = NULL;

  g_return_val_if_fail (VALENT_IS_DEVICE_TRANSFER (transfer), NULL);

  valent_object_lock (VALENT_OBJECT (transfer));
  if (transfer->file != NULL)
    ret = g_object_ref (transfer->file);
  valent_object_unlock (VALENT_OBJECT (transfer));

  return g_steal_pointer (&ret);
}

/**
 * valent_device_transfer_ref_packet: (get-property packet)
 * @transfer: a #ValentDeviceTransfer
 *
 * Get the KDE Connect packet.
 *
 * Returns: (transfer full) (nullable): a KDE Connect packet
 *
 * Since: 1.0
 */
JsonNode *
valent_device_transfer_ref_packet (ValentDeviceTransfer *transfer)
{
  g_autoptr (JsonNode) ret = NULL;

  g_return_val_if_fail (VALENT_IS_DEVICE_TRANSFER (transfer), NULL);

  valent_object_lock (VALENT_OBJECT (transfer));
  if (transfer->packet != NULL)
    ret = json_node_ref (transfer->packet);
  valent_object_unlock (VALENT_OBJECT (transfer));

  return g_steal_pointer (&ret);
}

