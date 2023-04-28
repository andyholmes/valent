// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-clipboard-adapter"

#include "config.h"

#include <gio/gio.h>
#include <libvalent-core.h>

#include "valent-clipboard-adapter.h"


/**
 * ValentClipboardAdapter:
 *
 * An abstract base class for clipboard selections.
 *
 * #ValentClipboardAdapter is a base class for plugins that provide an interface
 * to the desktop clipboard. This usually means reading and writing content,
 * including notification of content changes.
 *
 * ## `.plugin` File
 *
 * Implementations may define the following extra fields in the `.plugin` file:
 *
 * - `X-ClipboardAdapterPriority`
 *
 *     An integer indicating the adapter priority. The implementation with the
 *     lowest value will be used as the primary adapter.
 *
 * Since: 1.0
 */

typedef struct
{
  int64_t  timestamp;
} ValentClipboardAdapterPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (ValentClipboardAdapter, valent_clipboard_adapter, VALENT_TYPE_EXTENSION)

/**
 * ValentClipboardAdapterClass:
 * @changed: class closure for #ValentClipboardAdapter::changed signal
 * @get_mimetypes: the virtual function pointer for valent_clipboard_adapter_get_mimetypes()
 * @get_timestamp: the virtual function pointer for valent_clipboard_adapter_get_timestamp()
 * @read_bytes: the virtual function pointer for valent_clipboard_adapter_read_bytes()
 * @read_bytes_finish: the virtual function pointer for valent_clipboard_adapter_read_bytes_finish()
 * @write_bytes: the virtual function pointer for valent_clipboard_adapter_write_bytes()
 * @write_bytes_finish: the virtual function pointer for valent_clipboard_adapter_write_bytes_finish()
 *
 * The virtual function table for #ValentClipboardAdapter.
 */

enum {
  CHANGED,
  N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0, };


/* LCOV_EXCL_START */
static GStrv
valent_clipboard_adapter_real_get_mimetypes (ValentClipboardAdapter *adapter)
{
  g_assert (VALENT_IS_CLIPBOARD_ADAPTER (adapter));

  g_warning ("%s does not implement get_mimetypes",
             G_OBJECT_TYPE_NAME (adapter));

  return NULL;
}

static int64_t
valent_clipboard_adapter_real_get_timestamp (ValentClipboardAdapter *adapter)
{
  ValentClipboardAdapterPrivate *priv = valent_clipboard_adapter_get_instance_private (adapter);

  g_assert (VALENT_IS_CLIPBOARD_ADAPTER (adapter));

  return priv->timestamp;
}

static void
valent_clipboard_adapter_real_read_bytes (ValentClipboardAdapter *adapter,
                                          const char             *mimetype,
                                          GCancellable           *cancellable,
                                          GAsyncReadyCallback     callback,
                                          gpointer                user_data)
{
  g_task_report_new_error (adapter, callback, user_data,
                           valent_clipboard_adapter_real_read_bytes,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "%s does not implement read_bytes",
                           G_OBJECT_TYPE_NAME (adapter));
}

GBytes *
valent_clipboard_adapter_real_read_bytes_finish (ValentClipboardAdapter  *adapter,
                                                 GAsyncResult            *result,
                                                 GError                 **error)
{
  g_assert (VALENT_IS_CLIPBOARD_ADAPTER (adapter));
  g_assert (g_task_is_valid (result, adapter));
  g_assert (error == NULL || *error == NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
valent_clipboard_adapter_real_write_bytes (ValentClipboardAdapter *adapter,
                                           const char             *mimetype,
                                           GBytes                 *bytes,
                                           GCancellable           *cancellable,
                                           GAsyncReadyCallback     callback,
                                           gpointer                user_data)
{
  g_assert (VALENT_IS_CLIPBOARD_ADAPTER (adapter));
  g_assert (bytes == NULL || (mimetype != NULL && *mimetype != '\0'));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  g_task_report_new_error (adapter, callback, user_data,
                           valent_clipboard_adapter_real_write_bytes,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "%s does not implement write_bytes",
                           G_OBJECT_TYPE_NAME (adapter));
}

static gboolean
valent_clipboard_adapter_real_write_bytes_finish (ValentClipboardAdapter  *adapter,
                                                  GAsyncResult            *result,
                                                  GError                 **error)
{
  g_assert (VALENT_IS_CLIPBOARD_ADAPTER (adapter));
  g_assert (g_task_is_valid (result, adapter));
  g_assert (error == NULL || *error == NULL);

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
valent_clipboard_adapter_real_changed (ValentClipboardAdapter *adapter)
{
  ValentClipboardAdapterPrivate *priv = valent_clipboard_adapter_get_instance_private (adapter);

  g_assert (VALENT_IS_CLIPBOARD_ADAPTER (adapter));

  priv->timestamp = valent_timestamp_ms ();
}
/* LCOV_EXCL_STOP */

/*
 * GObject
 */
static void
valent_clipboard_adapter_class_init (ValentClipboardAdapterClass *klass)
{
  klass->changed = valent_clipboard_adapter_real_changed;
  klass->get_mimetypes = valent_clipboard_adapter_real_get_mimetypes;
  klass->get_timestamp = valent_clipboard_adapter_real_get_timestamp;
  klass->read_bytes = valent_clipboard_adapter_real_read_bytes;
  klass->read_bytes_finish = valent_clipboard_adapter_real_read_bytes_finish;
  klass->write_bytes = valent_clipboard_adapter_real_write_bytes;
  klass->write_bytes_finish = valent_clipboard_adapter_real_write_bytes_finish;

  /**
   * ValentClipboardAdapter::changed:
   * @adapter: a #ValentClipboardAdapter
   *
   * Emitted when the content of @adapter changes.
   *
   * The default handler for this signal updates the value returned by the
   * default implementation of [vfunc@Valent.ClipboardAdapter.get_timestamp].
   *
   * Since: 1.0
   */
  signals [CHANGED] =
    g_signal_new ("changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (ValentClipboardAdapterClass, changed),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);
}

static void
valent_clipboard_adapter_init (ValentClipboardAdapter *adapter)
{
}

/**
 * valent_clipboard_adapter_changed: (virtual changed)
 * @adapter: a #ValentClipboardAdapter
 *
 * Emits [signal@Valent.ClipboardAdapter::changed] signal on @adapter.
 *
 * The default handler for this signal updates the value returned by the default
 * implementation of [vfunc@Valent.ClipboardAdapter.get_timestamp].
 *
 * This method should only be called by implementations of
 * [class@Valent.ClipboardAdapter].
 *
 * Since: 1.0
 */
void
valent_clipboard_adapter_changed (ValentClipboardAdapter *adapter)
{
  g_return_if_fail (VALENT_IS_CLIPBOARD_ADAPTER (adapter));

  g_signal_emit (G_OBJECT (adapter), signals [CHANGED], 0);
}

/**
 * valent_clipboard_adapter_get_timestamp: (virtual get_timestamp)
 * @adapter: a #ValentClipboardAdapter
 *
 * Get the timestamp of the current clipboard content.
 *
 * The default implementation of this method returns the last time
 * [signal@Valent.ClipboardAdapter::changed] was emitted
 *
 * Returns: a UNIX epoch timestamp (ms)
 *
 * Since: 1.0
 */
int64_t
valent_clipboard_adapter_get_timestamp (ValentClipboardAdapter *adapter)
{
  int64_t ret;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_CLIPBOARD_ADAPTER (adapter), 0);

  ret = VALENT_CLIPBOARD_ADAPTER_GET_CLASS (adapter)->get_timestamp (adapter);

  VALENT_RETURN (ret);
}

/**
 * valent_clipboard_adapter_get_mimetypes: (virtual get_mimetypes)
 * @adapter: a #ValentClipboardAdapter
 *
 * Get the mime-types of the current clipboard content.
 *
 * Returns: (transfer full) (nullable) (array zero-terminated=1): a list of
 *   mime-types
 *
 * Since: 1.0
 */
GStrv
valent_clipboard_adapter_get_mimetypes (ValentClipboardAdapter *adapter)
{
  GStrv ret = NULL;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_CLIPBOARD_ADAPTER (adapter), NULL);

  ret = VALENT_CLIPBOARD_ADAPTER_GET_CLASS (adapter)->get_mimetypes (adapter);

  VALENT_RETURN (g_steal_pointer (&ret));
}

/**
 * valent_clipboard_adapter_read_bytes: (virtual read_bytes)
 * @adapter: a #ValentClipboardAdapter
 * @mimetype: a mime-type
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): a #GAsyncReadyCallback
 * @user_data: (closure): user supplied data
 *
 * Get the content of @adapter.
 *
 * Call [method@Valent.ClipboardAdapter.read_bytes_finish] to get the result.
 *
 * Since: 1.0
 */
void
valent_clipboard_adapter_read_bytes (ValentClipboardAdapter *adapter,
                                     const char             *mimetype,
                                     GCancellable           *cancellable,
                                     GAsyncReadyCallback     callback,
                                     gpointer                user_data)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_CLIPBOARD_ADAPTER (adapter));
  g_return_if_fail (mimetype != NULL && *mimetype != '\0');
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  VALENT_CLIPBOARD_ADAPTER_GET_CLASS (adapter)->read_bytes (adapter,
                                                            mimetype,
                                                            cancellable,
                                                            callback,
                                                            user_data);
  VALENT_EXIT;
}

/**
 * valent_clipboard_adapter_read_bytes_finish: (virtual read_bytes_finish)
 * @adapter: a #ValentClipboardAdapter
 * @result: a #GAsyncResult
 * @error: (nullable): a #GError
 *
 * Finish an operation started by [method@Valent.ClipboardAdapter.read_bytes].
 *
 * Returns: (transfer full) (nullable): a #GBytes, or %NULL with @error set
 *
 * Since: 1.0
 */
GBytes *
valent_clipboard_adapter_read_bytes_finish (ValentClipboardAdapter  *adapter,
                                            GAsyncResult            *result,
                                            GError                 **error)
{
  GBytes *ret;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_CLIPBOARD_ADAPTER (adapter), NULL);
  g_return_val_if_fail (g_task_is_valid (result, adapter), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  ret = VALENT_CLIPBOARD_ADAPTER_GET_CLASS (adapter)->read_bytes_finish (adapter,
                                                                         result,
                                                                         error);

  VALENT_RETURN (g_steal_pointer (&ret));
}

/**
 * valent_clipboard_adapter_write_bytes: (virtual write_bytes)
 * @adapter: a #ValentClipboardAdapter
 * @mimetype: (nullable): a mime-type, or %NULL if @bytes is %NULL
 * @bytes: (nullable): a #GBytes, or %NULL if @mimetype is %NULL
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): a #GAsyncReadyCallback
 * @user_data: (closure): user supplied data
 *
 * Set the content of the clipboard.
 *
 * Call [method@Valent.ClipboardAdapter.write_bytes_finish] to get the result.
 *
 * Since: 1.0
 */
void
valent_clipboard_adapter_write_bytes (ValentClipboardAdapter *adapter,
                                      const char             *mimetype,
                                      GBytes                 *bytes,
                                      GCancellable           *cancellable,
                                      GAsyncReadyCallback     callback,
                                      gpointer                user_data)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_CLIPBOARD_ADAPTER (adapter));
  g_return_if_fail (bytes == NULL || (mimetype != NULL && *mimetype != '\0'));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  VALENT_CLIPBOARD_ADAPTER_GET_CLASS (adapter)->write_bytes (adapter,
                                                             mimetype,
                                                             bytes,
                                                             cancellable,
                                                             callback,
                                                             user_data);

  VALENT_EXIT;
}

/**
 * valent_clipboard_adapter_write_bytes_finish: (virtual write_bytes_finish)
 * @adapter: a #ValentClipboardAdapter
 * @result: a #GAsyncResult
 * @error: (nullable): a #GError
 *
 * Finish an operation started by [method@Valent.ClipboardAdapter.write_bytes].
 *
 * Returns: %TRUE if successful, or %FALSE with @error set
 *
 * Since: 1.0
 */
gboolean
valent_clipboard_adapter_write_bytes_finish (ValentClipboardAdapter  *adapter,
                                             GAsyncResult            *result,
                                             GError                 **error)
{
  gboolean ret;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_CLIPBOARD_ADAPTER (adapter), FALSE);
  g_return_val_if_fail (g_task_is_valid (result, adapter), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  ret = VALENT_CLIPBOARD_ADAPTER_GET_CLASS (adapter)->write_bytes_finish (adapter,
                                                                          result,
                                                                          error);

  VALENT_RETURN (ret);
}

