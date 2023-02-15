// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-share-upload"

#include "config.h"

#include <gio/gio.h>
#include <libvalent-core.h>
#include <libvalent-device.h>

#include "valent-share-upload.h"


/**
 * ValentShareUpload:
 *
 * A class for multi-file uploads.
 *
 * #ValentShareUpload is a class that supports multi-file uploads for
 * #ValentSharePlugin.
 */

struct _ValentShareUpload
{
  ValentTransfer  parent_instance;

  ValentDevice   *device;
  GPtrArray      *items;

  unsigned int    position;
  unsigned int    processing_files;
  goffset         payload_size;
};

static void       g_list_model_iface_init  (GListModelInterface *iface);
static gboolean   valent_share_upload_idle (gpointer             data);

G_DEFINE_TYPE_WITH_CODE (ValentShareUpload, valent_share_upload, VALENT_TYPE_TRANSFER,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, g_list_model_iface_init))

enum {
  PROP_0,
  PROP_DEVICE,
  N_PROPERTIES,
};

static GParamSpec *properties[N_PROPERTIES] = { 0, };


static void
valent_share_upload_update (ValentShareUpload *self)
{
  g_autoptr (JsonNode) packet = NULL;
  g_autoptr (JsonBuilder) builder = NULL;

  g_assert (VALENT_IS_SHARE_UPLOAD (self));

  valent_packet_init (&builder, "kdeconnect.share.request.update");
  json_builder_set_member_name (builder, "numberOfFiles");
  json_builder_add_int_value (builder, self->items->len);
  json_builder_set_member_name (builder, "totalPayloadSize");
  json_builder_add_int_value (builder, self->payload_size);
  packet = valent_packet_end (&builder);

  valent_device_queue_packet (self->device, packet);
}

static inline void
valent_share_upload_update_transfer (ValentShareUpload *self,
                                     ValentTransfer    *transfer)
{
  g_autoptr (JsonNode) packet = NULL;
  JsonObject *body;

  g_assert (VALENT_IS_SHARE_UPLOAD (self));
  g_assert (VALENT_IS_TRANSFER (transfer));

  packet = valent_device_transfer_ref_packet (VALENT_DEVICE_TRANSFER (transfer));
  body = valent_packet_get_body (packet);
  json_object_set_int_member (body, "numberOfFiles", self->items->len);
  json_object_set_int_member (body, "totalPayloadSize", self->payload_size);
}

static void
valent_transfer_execute_cb (GObject      *object,
                            GAsyncResult *result,
                            gpointer      user_data)
{
  ValentTransfer *transfer = VALENT_TRANSFER (object);
  ValentShareUpload *self = g_task_get_source_object (G_TASK (user_data));
  g_autoptr (GTask) task = G_TASK (user_data);
  g_autoptr (GError) error = NULL;

  if (!valent_transfer_execute_finish (transfer, result, &error))
    return g_task_return_error (task, g_steal_pointer (&error));

  if (self->position < self->items->len)
    {
      ValentTransfer *item = g_ptr_array_index (self->items, self->position++);

      valent_share_upload_update_transfer (self, item);
      valent_transfer_execute (item,
                               g_task_get_cancellable (task),
                               valent_transfer_execute_cb,
                               g_object_ref (task));
    }
  else if (self->processing_files)
    {
      g_autoptr (GSource) source = NULL;

      source = g_idle_source_new ();
      g_task_attach_source (task, source, valent_share_upload_idle);
    }
  else
    {
      g_task_return_boolean (task, TRUE);
    }
}

static gboolean
valent_share_upload_idle (gpointer data)
{
  ValentShareUpload *self = g_task_get_source_object (G_TASK (data));
  GTask *task = G_TASK (data);

  if (g_task_return_error_if_cancelled (task))
    return G_SOURCE_REMOVE;

  if (self->position < self->items->len)
    {
      ValentTransfer *item = g_ptr_array_index (self->items, self->position++);

      valent_share_upload_update_transfer (self, item);
      valent_transfer_execute (item,
                               g_task_get_cancellable (task),
                               valent_transfer_execute_cb,
                               g_object_ref (task));

      return G_SOURCE_REMOVE;
    }
  else if (self->processing_files)
    {
      return G_SOURCE_CONTINUE;
    }

  g_task_return_boolean (task, TRUE);
  return G_SOURCE_REMOVE;
}

static void
valent_share_upload_execute (ValentTransfer      *transfer,
                             GCancellable        *cancellable,
                             GAsyncReadyCallback  callback,
                             gpointer             user_data)
{
  ValentShareUpload *self = VALENT_SHARE_UPLOAD (transfer);
  g_autoptr (GTask) task = NULL;
  g_autoptr (GSource) source = NULL;

  g_assert (VALENT_IS_SHARE_UPLOAD (self));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  source = g_idle_source_new ();

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_share_upload_execute);
  g_task_attach_source (task, source, valent_share_upload_idle);
}

/*
 * GListModel
 */
static gpointer
valent_share_upload_get_item (GListModel   *model,
                              unsigned int  position)
{
  ValentShareUpload *self = VALENT_SHARE_UPLOAD (model);

  g_assert (VALENT_SHARE_UPLOAD (self));

  if G_UNLIKELY (position >= self->items->len)
    return NULL;

  return g_object_ref (g_ptr_array_index (self->items, position));
}

static GType
valent_share_upload_get_item_type (GListModel *model)
{
  return VALENT_TYPE_TRANSFER;
}

static unsigned int
valent_share_upload_get_n_items (GListModel *model)
{
  ValentShareUpload *self = VALENT_SHARE_UPLOAD (model);

  g_assert (VALENT_SHARE_UPLOAD (self));

  return self->items->len;
}

static void
g_list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item = valent_share_upload_get_item;
  iface->get_item_type = valent_share_upload_get_item_type;
  iface->get_n_items = valent_share_upload_get_n_items;
}

/*
 * GObject
 */
static void
valent_share_upload_finalize (GObject *object)
{
  ValentShareUpload *self = VALENT_SHARE_UPLOAD (object);

  valent_object_lock (VALENT_OBJECT (self));
  g_clear_object (&self->device);
  g_clear_pointer (&self->items, g_ptr_array_unref);
  valent_object_unlock (VALENT_OBJECT (self));

  G_OBJECT_CLASS (valent_share_upload_parent_class)->finalize (object);
}

static void
valent_share_upload_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  ValentShareUpload *self = VALENT_SHARE_UPLOAD (object);

  switch (prop_id)
    {
    case PROP_DEVICE:
      valent_object_lock (VALENT_OBJECT (self));
      g_value_set_object (value, self->device);
      valent_object_unlock (VALENT_OBJECT (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_share_upload_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  ValentShareUpload *self = VALENT_SHARE_UPLOAD (object);

  switch (prop_id)
    {
    case PROP_DEVICE:
      valent_object_lock (VALENT_OBJECT (self));
      self->device = g_value_dup_object (value);
      valent_object_unlock (VALENT_OBJECT (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_share_upload_class_init (ValentShareUploadClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentTransferClass *transfer_class = VALENT_TRANSFER_CLASS (klass);

  object_class->finalize = valent_share_upload_finalize;
  object_class->get_property = valent_share_upload_get_property;
  object_class->set_property = valent_share_upload_set_property;

  transfer_class->execute = valent_share_upload_execute;

  /**
   * ValentShareUpload:device:
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
valent_share_upload_init (ValentShareUpload *self)
{
  self->items = g_ptr_array_new_with_free_func (g_object_unref);
}

/**
 * valent_share_upload_new:
 * @device: a #ValentDevice
 *
 * Create a new #ValentShareUpload.
 *
 * Returns: (transfer full): a new #ValentShareUpload
 */
ValentTransfer *
valent_share_upload_new (ValentDevice *device)
{
  g_return_val_if_fail (VALENT_IS_DEVICE (device), NULL);

  return g_object_new (VALENT_TYPE_SHARE_UPLOAD,
                       "device", device,
                       NULL);
}

static void
valent_share_upload_add_files_cb (GObject      *object,
                                  GAsyncResult *result,
                                  gpointer      user_data)
{
  ValentShareUpload *self = VALENT_SHARE_UPLOAD (object);
  g_autoptr (GPtrArray) items = NULL;
  unsigned int position, added;
  g_autoptr (GError) error = NULL;

  VALENT_ENTRY;

  g_assert (VALENT_IS_SHARE_UPLOAD (self));
  g_assert (g_task_is_valid (result, self));

  if ((items = g_task_propagate_pointer (G_TASK (result), &error)) == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s: %s", G_OBJECT_TYPE_NAME (self), error->message);

      self->processing_files--;
      VALENT_EXIT;
    }

  position = self->items->len;
  added = items->len;

  for (unsigned int i = 0; i < items->len; i++)
    {
      ValentDeviceTransfer *transfer = g_ptr_array_index (items, i);
      g_autoptr (JsonNode) packet = NULL;

      packet = valent_device_transfer_ref_packet (transfer);
      self->payload_size += valent_packet_get_payload_size (packet);
    }

  g_ptr_array_extend_and_steal (self->items, g_steal_pointer (&items));
  self->processing_files--;

  g_list_model_items_changed (G_LIST_MODEL (self), position, 0, added);
  valent_share_upload_update (self);

  VALENT_EXIT;
}

static void
valent_share_upload_add_files_task (GTask        *task,
                                    gpointer      source_object,
                                    gpointer      task_data,
                                    GCancellable *cancellable)
{
  ValentShareUpload *self = VALENT_SHARE_UPLOAD (source_object);
  GPtrArray *files = task_data;
  g_autoptr (ValentDevice) device = NULL;
  g_autoptr (GPtrArray) items = NULL;

  g_assert (VALENT_IS_SHARE_UPLOAD (self));
  g_assert (files != NULL);

  if (g_task_return_error_if_cancelled (task))
    return;

  valent_object_lock (VALENT_OBJECT (self));
  device = g_object_ref (self->device);
  valent_object_unlock (VALENT_OBJECT (self));

  items = g_ptr_array_new_with_free_func (g_object_unref);

  for (unsigned int i = 0; i < files->len; i++)
    {
      GFile *file = g_ptr_array_index (files, i);
      g_autoptr (ValentTransfer) transfer = NULL;
      g_autoptr (GFileInfo) info = NULL;
      g_autoptr (JsonNode) packet = NULL;
      g_autoptr (JsonBuilder) builder = NULL;
      const char *filename;
      goffset payload_size;
      g_autoptr (GError) error = NULL;

      info = g_file_query_info (file,
                                G_FILE_ATTRIBUTE_STANDARD_NAME","
                                G_FILE_ATTRIBUTE_STANDARD_SIZE,
                                G_FILE_QUERY_INFO_NONE,
                                cancellable,
                                &error);

      if (info == NULL)
        return g_task_return_error (task, g_steal_pointer (&error));

      filename = g_file_info_get_name (info);
      payload_size = g_file_info_get_size (info);

      valent_packet_init (&builder, "kdeconnect.share.request");
      json_builder_set_member_name (builder, "filename");
      json_builder_add_string_value (builder, filename);
      json_builder_set_member_name (builder, "open");
      json_builder_add_boolean_value (builder, FALSE);
      packet = valent_packet_end (&builder);

      valent_packet_set_payload_size (packet, payload_size);

      transfer = valent_device_transfer_new_for_file (device, packet, file);
      g_ptr_array_add (items, g_steal_pointer (&transfer));
    }

  g_task_return_pointer (task, g_steal_pointer (&items),
                         (GDestroyNotify)g_ptr_array_unref);
}

/**
 * valent_share_upload_add_file:
 * @group: a #ValentShareUpload
 * @file: a #GFile
 *
 * Add @file to the transfer operation.
 */
void
valent_share_upload_add_file (ValentShareUpload *upload,
                              GFile             *file)
{
  g_autoptr (GTask) task = NULL;
  g_autoptr (GCancellable) destroy = NULL;
  g_autoptr (GPtrArray) items = NULL;

  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_SHARE_UPLOAD (upload));
  g_return_if_fail (G_IS_FILE (file));

  upload->processing_files++;

  items = g_ptr_array_new_full (1, g_object_unref);
  g_ptr_array_add (items, g_object_ref (file));

  destroy = valent_object_ref_cancellable (VALENT_OBJECT (upload));
  task = g_task_new (upload, destroy, valent_share_upload_add_files_cb, NULL);
  g_task_set_source_tag (task, valent_share_upload_add_file);
  g_task_set_task_data (task,
                        g_steal_pointer (&items),
                        (GDestroyNotify)g_ptr_array_unref);
  g_task_run_in_thread (task, valent_share_upload_add_files_task);

  VALENT_EXIT;
}

/**
 * valent_share_upload_add_files:
 * @group: a #ValentShareUpload
 * @files: a #GListModel
 *
 * Add @files to the transfer operation.
 *
 * The [property@Gio.ListModel:item-type] of @files must be [type@Gio.File].
 *
 * Call [method@Valent.ShareUpload.add_files_finish] to get the result.
 */
void
valent_share_upload_add_files (ValentShareUpload *upload,
                               GListModel        *files)
{
  g_autoptr (GTask) task = NULL;
  g_autoptr (GCancellable) destroy = NULL;
  g_autoptr (GPtrArray) items = NULL;
  unsigned int n_files = 0;

  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_SHARE_UPLOAD (upload));
  g_return_if_fail (G_IS_LIST_MODEL (files));
  g_return_if_fail (g_list_model_get_item_type (files) == G_TYPE_FILE);

  upload->processing_files++;

  n_files = g_list_model_get_n_items (files);
  items = g_ptr_array_new_full (n_files, g_object_unref);

  for (unsigned int i = 0; i < n_files; i++)
    g_ptr_array_add (items, g_list_model_get_item (files, i));

  destroy = valent_object_ref_cancellable (VALENT_OBJECT (upload));
  task = g_task_new (upload, destroy, valent_share_upload_add_files_cb, NULL);
  g_task_set_source_tag (task, valent_share_upload_add_files);
  g_task_set_task_data (task,
                        g_steal_pointer (&items),
                        (GDestroyNotify)g_ptr_array_unref);
  g_task_run_in_thread (task, valent_share_upload_add_files_task);

  VALENT_EXIT;
}

