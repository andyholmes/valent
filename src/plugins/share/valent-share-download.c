// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-share-download"

#include "config.h"

#include <gio/gio.h>
#include <valent.h>

#include "valent-share-download.h"

/* The maximum time in milliseconds to wait for the next expected transfer item,
 * allowing for the gap between one file completing and the packet for the next.
 *
 * The current timeout matches kdeconnect-android which waits 1000ms before
 * reporting an error, while kdeconnect-kde has no wait period. */
#define OPERATION_TIMEOUT_MS 1000


/**
 * ValentShareDownload:
 *
 * A class for multi-file downloads.
 *
 * #ValentShareDownload is a class that supports multi-file downloads for
 * #ValentSharePlugin.
 */

struct _ValentShareDownload
{
  ValentTransfer  parent_instance;

  ValentDevice   *device;
  GPtrArray      *items;

  unsigned int    position;
  int64_t         number_of_files;
  goffset         payload_size;
};

static void       g_list_model_iface_init       (GListModelInterface *iface);
static gboolean   valent_share_download_timeout (gpointer             data);

G_DEFINE_FINAL_TYPE_WITH_CODE (ValentShareDownload, valent_share_download, VALENT_TYPE_TRANSFER,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, g_list_model_iface_init))

enum {
  PROP_0,
  PROP_DEVICE,
  N_PROPERTIES,
};

static GParamSpec *properties[N_PROPERTIES] = { 0, };


/*
 * ValentTransfer
 */
static void
valent_transfer_execute_cb (GObject      *object,
                            GAsyncResult *result,
                            gpointer      user_data)
{
  ValentTransfer *transfer = VALENT_TRANSFER (object);
  ValentShareDownload *self = g_task_get_source_object (G_TASK (user_data));
  g_autoptr (GTask) task = G_TASK (user_data);
  g_autoptr (GError) error = NULL;

  if (!valent_transfer_execute_finish (transfer, result, &error))
    return g_task_return_error (task, g_steal_pointer (&error));

  if (self->position < self->items->len)
    {
      ValentTransfer *item = g_ptr_array_index (self->items, self->position++);

      valent_transfer_execute (item,
                               g_task_get_cancellable (task),
                               valent_transfer_execute_cb,
                               g_object_ref (task));
    }
  else if (self->position < self->number_of_files)
    {
      g_autoptr (GSource) source = NULL;

      source = g_timeout_source_new (OPERATION_TIMEOUT_MS);
      g_task_attach_source (task, source, valent_share_download_timeout);
    }
  else
    {
      g_task_return_boolean (task, TRUE);
    }
}

static gboolean
valent_share_download_timeout (gpointer data)
{
  ValentShareDownload *self = g_task_get_source_object (G_TASK (data));
  GTask *task = G_TASK (data);

  if (g_task_return_error_if_cancelled (task))
    return G_SOURCE_REMOVE;

  if (self->position < self->items->len)
    {
      ValentTransfer *item = g_ptr_array_index (self->items, self->position++);

      valent_transfer_execute (item,
                               g_task_get_cancellable (task),
                               valent_transfer_execute_cb,
                               g_object_ref (task));
    }
  else if (self->position < self->number_of_files)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_PARTIAL_INPUT,
                               "Failed to receive %u of %u files",
                               (unsigned int)self->number_of_files - self->position,
                               (unsigned int)self->number_of_files);
    }

  return G_SOURCE_REMOVE;
}

static void
valent_share_download_execute (ValentTransfer      *transfer,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
  ValentShareDownload *self = VALENT_SHARE_DOWNLOAD (transfer);
  g_autoptr (GTask) task = NULL;

  g_assert (VALENT_IS_SHARE_DOWNLOAD (self));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_share_download_execute);

  if (self->position < self->items->len)
    {
      ValentTransfer *item = g_ptr_array_index (self->items, self->position++);

      valent_transfer_execute (item,
                               g_task_get_cancellable (task),
                               valent_transfer_execute_cb,
                               g_object_ref (task));
    }
  else if (self->position < self->number_of_files)
    {
      g_autoptr (GSource) source = NULL;

      source = g_timeout_source_new (OPERATION_TIMEOUT_MS);
      g_task_attach_source (task, source, valent_share_download_timeout);
    }
  else
    {
      g_task_return_boolean (task, TRUE);
    }
}

/*
 * GListModel
 */
static gpointer
valent_share_download_get_item (GListModel   *model,
                                unsigned int  position)
{
  ValentShareDownload *self = VALENT_SHARE_DOWNLOAD (model);

  g_assert (VALENT_SHARE_DOWNLOAD (self));

  if G_UNLIKELY (position >= self->items->len)
    return NULL;

  return g_object_ref (g_ptr_array_index (self->items, position));
}

static GType
valent_share_download_get_item_type (GListModel *model)
{
  g_assert (VALENT_SHARE_DOWNLOAD (model));

  return VALENT_TYPE_TRANSFER;
}

static unsigned int
valent_share_download_get_n_items (GListModel *model)
{
  ValentShareDownload *self = VALENT_SHARE_DOWNLOAD (model);

  g_assert (VALENT_SHARE_DOWNLOAD (self));

  /* FIXME: this indicates the number of total transfers, not the number of
   *        items currently available in the list model. */
  return self->number_of_files;
}

static void
g_list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item = valent_share_download_get_item;
  iface->get_item_type = valent_share_download_get_item_type;
  iface->get_n_items = valent_share_download_get_n_items;
}

/*
 * GObject
 */
static void
valent_share_download_finalize (GObject *object)
{
  ValentShareDownload *self = VALENT_SHARE_DOWNLOAD (object);

  g_clear_object (&self->device);
  g_clear_pointer (&self->items, g_ptr_array_unref);

  G_OBJECT_CLASS (valent_share_download_parent_class)->finalize (object);
}

static void
valent_share_download_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  ValentShareDownload *self = VALENT_SHARE_DOWNLOAD (object);

  switch (prop_id)
    {
    case PROP_DEVICE:
      g_value_set_object (value, self->device);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_share_download_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  ValentShareDownload *self = VALENT_SHARE_DOWNLOAD (object);

  switch (prop_id)
    {
    case PROP_DEVICE:
      self->device = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_share_download_class_init (ValentShareDownloadClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentTransferClass *transfer_class = VALENT_TRANSFER_CLASS (klass);

  object_class->finalize = valent_share_download_finalize;
  object_class->get_property = valent_share_download_get_property;
  object_class->set_property = valent_share_download_set_property;

  transfer_class->execute = valent_share_download_execute;

  /**
   * ValentShareDownload:device:
   *
   * The [class@Valent.Device] this transfer is for.
   */
  properties [PROP_DEVICE] =
    g_param_spec_object ("device", NULL, NULL,
                         VALENT_TYPE_DEVICE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
valent_share_download_init (ValentShareDownload *self)
{
  self->items = g_ptr_array_new_with_free_func (g_object_unref);
}

/**
 * valent_share_download_new:
 * @device: a #ValentDevice
 *
 * Create a new #ValentShareDownload.
 *
 * Returns: (transfer full): a new #ValentShareDownload
 */
ValentTransfer *
valent_share_download_new (ValentDevice *device)
{
  g_return_val_if_fail (VALENT_IS_DEVICE (device), NULL);

  return g_object_new (VALENT_TYPE_SHARE_DOWNLOAD,
                       "device", device,
                       NULL);
}

/**
 * valent_share_download_add_file:
 * @group: a #ValentShareDownload
 * @file: a #GFile
 * @packet: a KDE Connect packet
 *
 * Add @file to the transfer operation.
 */
void
valent_share_download_add_file (ValentShareDownload *download,
                                GFile               *file,
                                JsonNode            *packet)
{
  g_autoptr (ValentTransfer) item = NULL;
  unsigned int position, added;
  int64_t number_of_files;
  goffset total_payload_size;

  g_return_if_fail (VALENT_IS_SHARE_DOWNLOAD (download));
  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (VALENT_IS_PACKET (packet));

  if (!valent_packet_get_int (packet, "numberOfFiles", &number_of_files))
    number_of_files = download->number_of_files + 1;

  if (!valent_packet_get_int (packet, "totalPayloadSize", &total_payload_size))
    total_payload_size = download->payload_size + valent_packet_get_payload_size (packet);

  position = download->items->len;
  added = number_of_files - download->number_of_files;

  download->number_of_files = number_of_files;
  download->payload_size = total_payload_size;

  item = valent_device_transfer_new (download->device, packet, file);
  g_ptr_array_add (download->items, g_steal_pointer (&item));

  /* FIXME: this indicates the number of total transfers, not the number of
   *        items currently available in the list model. */
  g_list_model_items_changed (G_LIST_MODEL (download), position, 0, added);
}

/**
 * valent_share_download_update:
 * @download: a #ValentShareDownload
 * @packet: a KDE Connect packet
 *
 * Update the number of files and total payload size for @download.
 */
void
valent_share_download_update (ValentShareDownload *self,
                              JsonNode            *packet)
{
  g_return_if_fail (VALENT_IS_SHARE_DOWNLOAD (self));
  g_return_if_fail (VALENT_IS_PACKET (packet));

  if (!valent_packet_get_int (packet, "numberOfFiles", &self->number_of_files))
    {
      g_debug ("%s(): expected \"numberOfFiles\" field holding an integer",
               G_STRFUNC);
      return;
    }

  if (!valent_packet_get_int (packet, "totalPayloadSize", &self->payload_size))
    {
      g_debug ("%s(): expected \"totalPayloadSize\" field holding an integer",
               G_STRFUNC);
      return;
    }
}

