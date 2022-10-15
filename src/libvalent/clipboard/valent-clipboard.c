// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-clipboard"

#include "config.h"

#include <gio/gio.h>
#include <libpeas/peas.h>
#include <libvalent-core.h>

#include "valent-clipboard.h"
#include "valent-clipboard-adapter.h"


/**
 * ValentClipboard:
 *
 * A class for reading and writing the desktop clipboard.
 *
 * #ValentClipboard is an abstraction of clipboard selections, intended for use
 * by [class@Valent.DevicePlugin] implementations.
 *
 * Plugins can implement [class@Valent.ClipboardAdapter] to provide an interface
 * to access a clipboard selection.
 *
 * Since: 1.0
 */

struct _ValentClipboard
{
  ValentComponent         parent_instance;

  ValentClipboardAdapter *default_adapter;
};

G_DEFINE_TYPE (ValentClipboard, valent_clipboard, VALENT_TYPE_COMPONENT)

enum {
  CHANGED,
  N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0, };

static ValentClipboard *default_clipboard = NULL;


static void
valent_clipboard_adapter_read_bytes_cb (ValentClipboardAdapter *adapter,
                                        GAsyncResult           *result,
                                        gpointer                user_data)
{
  g_autoptr (GTask) task = G_TASK (user_data);
  g_autoptr (GBytes) bytes = NULL;
  g_autoptr (GError) error = NULL;

  g_assert (VALENT_IS_CLIPBOARD_ADAPTER (adapter));
  g_assert (g_task_is_valid (result, adapter));

  bytes = valent_clipboard_adapter_read_bytes_finish (adapter, result, &error);

  if (bytes == NULL)
    return g_task_return_error (task, g_steal_pointer (&error));

  g_task_return_pointer (task,
                         g_steal_pointer (&bytes),
                         (GDestroyNotify)g_bytes_unref);
}

static void
valent_clipboard_adapter_write_bytes_cb (ValentClipboardAdapter *adapter,
                                         GAsyncResult           *result,
                                         gpointer                user_data)
{
  g_autoptr (GTask) task = G_TASK (user_data);
  g_autoptr (GError) error = NULL;

  g_assert (VALENT_IS_CLIPBOARD_ADAPTER (adapter));
  g_assert (g_task_is_valid (result, adapter));

  if (!valent_clipboard_adapter_write_bytes_finish (adapter, result, &error))
    return g_task_return_error (task, g_steal_pointer (&error));

  g_task_return_boolean (task, TRUE);
}

static void
valent_clipboard_adapter_read_text_cb (ValentClipboardAdapter *adapter,
                                       GAsyncResult           *result,
                                       gpointer                user_data)
{
  g_autoptr (GTask) task = G_TASK (user_data);
  g_autoptr (GError) error = NULL;
  g_autofree char *text = NULL;

  g_assert (VALENT_IS_CLIPBOARD_ADAPTER (adapter));
  g_assert (g_task_is_valid (result, adapter));

  text = valent_clipboard_adapter_read_text_finish (adapter, result, &error);

  if (text == NULL)
    return g_task_return_error (task, g_steal_pointer (&error));

  g_task_return_pointer (task, g_steal_pointer (&text), g_free);
}

static void
valent_clipboard_adapter_write_text_cb (ValentClipboardAdapter *adapter,
                                        GAsyncResult           *result,
                                        gpointer                user_data)
{
  g_autoptr (GTask) task = G_TASK (user_data);
  g_autoptr (GError) error = NULL;

  g_assert (VALENT_IS_CLIPBOARD_ADAPTER (adapter));
  g_assert (g_task_is_valid (result, adapter));

  if (!valent_clipboard_adapter_write_text_finish (adapter, result, &error))
    return g_task_return_error (task, g_steal_pointer (&error));

  g_task_return_boolean (task, TRUE);
}

static void
on_clipboard_adapter_changed (ValentClipboardAdapter *clipboard,
                              ValentClipboard        *self)
{
  VALENT_ENTRY;

  if (self->default_adapter == clipboard)
    g_signal_emit (G_OBJECT (self), signals [CHANGED], 0);

  VALENT_EXIT;
}

/*
 * ValentComponent
 */
static void
valent_clipboard_bind_preferred (ValentComponent *component,
                                 PeasExtension   *extension)
{
  ValentClipboard *self = VALENT_CLIPBOARD (component);
  ValentClipboardAdapter *adapter = VALENT_CLIPBOARD_ADAPTER (extension);

  VALENT_ENTRY;

  g_assert (VALENT_IS_CLIPBOARD (self));
  g_assert (adapter == NULL || VALENT_IS_CLIPBOARD_ADAPTER (adapter));

  if (self->default_adapter != NULL)
    {
      g_signal_handlers_disconnect_by_data (self->default_adapter, self);
      self->default_adapter = NULL;
    }

  if (adapter != NULL)
    {
      self->default_adapter = adapter;
      g_signal_connect_object (self->default_adapter,
                               "changed",
                               G_CALLBACK (on_clipboard_adapter_changed),
                               self, 0);
    }

  VALENT_EXIT;
}

/*
 * GObject
 */
static void
valent_clipboard_class_init (ValentClipboardClass *klass)
{
  ValentComponentClass *component_class = VALENT_COMPONENT_CLASS (klass);

  component_class->bind_preferred = valent_clipboard_bind_preferred;

  /**
   * ValentClipboard::changed:
   * @clipboard: a #ValentClipboard
   *
   * Emitted when the content of the primary [class@Valent.ClipboardAdapter]
   * changes.
   *
   * Since: 1.0
   */
  signals [CHANGED] =
    g_signal_new ("changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);
}

static void
valent_clipboard_init (ValentClipboard *self)
{
}

/**
 * valent_clipboard_get_default:
 *
 * Get the default [class@Valent.Clipboard].
 *
 * Returns: (transfer none) (not nullable): a #ValentClipboard
 *
 * Since: 1.0
 */
ValentClipboard *
valent_clipboard_get_default (void)
{
  if (default_clipboard == NULL)
    {
      default_clipboard = g_object_new (VALENT_TYPE_CLIPBOARD,
                                        "plugin-context",  "clipboard",
                                        "plugin-priority", "ClipboardAdapterPriority",
                                        "plugin-type",     VALENT_TYPE_CLIPBOARD_ADAPTER,
                                        NULL);

      g_object_add_weak_pointer (G_OBJECT (default_clipboard),
                                 (gpointer)&default_clipboard);
    }

  return default_clipboard;
}

/**
 * valent_clipboard_get_mimetypes:
 * @clipboard: a #ValentClipboard
 *
 * Get the mime-types of the primary clipboard content.
 *
 * Returns: (transfer full) (nullable) (array zero-terminated=1): a list of
 *   mime-types
 *
 * Since: 1.0
 */
GStrv
valent_clipboard_get_mimetypes (ValentClipboard *clipboard)
{
  GStrv ret = NULL;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_CLIPBOARD (clipboard), NULL);

  if G_LIKELY (clipboard->default_adapter != NULL)
    ret = valent_clipboard_adapter_get_mimetypes (clipboard->default_adapter);

  VALENT_RETURN (g_steal_pointer (&ret));
}

/**
 * valent_clipboard_get_timestamp:
 * @clipboard: a #ValentClipboard
 *
 * Get the timestamp of the current clipboard content, in milliseconds since the
 * UNIX epoch.
 *
 * Returns: a UNIX epoch timestamp (ms)
 *
 * Since: 1.0
 */
gint64
valent_clipboard_get_timestamp (ValentClipboard *clipboard)
{
  gint64 ret = 0;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_CLIPBOARD (clipboard), 0);

  if G_LIKELY (clipboard->default_adapter != NULL)
    ret = valent_clipboard_adapter_get_timestamp (clipboard->default_adapter);

  VALENT_RETURN (ret);
}

/**
 * valent_clipboard_read_bytes:
 * @clipboard: a #ValentClipboard
 * @mimetype: a mime-type
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): a #GAsyncReadyCallback
 * @user_data: (closure): user supplied data
 *
 * Get the content of the primary clipboard adapter.
 *
 * Call [method@Valent.Clipboard.read_bytes_finish] to get the result.
 *
 * Since: 1.0
 */
void
valent_clipboard_read_bytes (ValentClipboard     *clipboard,
                             const char          *mimetype,
                             GCancellable        *cancellable,
                             GAsyncReadyCallback  callback,
                             gpointer             user_data)
{
  g_autoptr (GTask) task = NULL;

  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_CLIPBOARD (clipboard));
  g_return_if_fail (mimetype != NULL && *mimetype != '\0');
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  if G_UNLIKELY (clipboard->default_adapter == NULL)
    {
      g_task_report_new_error (clipboard, callback, user_data,
                               valent_clipboard_read_bytes,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_SUPPORTED,
                               "No clipboard adapter available");
      return;
    }

  task = g_task_new (clipboard, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_clipboard_read_bytes);
  valent_clipboard_adapter_read_bytes (clipboard->default_adapter,
                                      mimetype,
                                      cancellable,
                                      (GAsyncReadyCallback)valent_clipboard_adapter_read_bytes_cb,
                                      g_steal_pointer (&task));

  VALENT_EXIT;
}

/**
 * valent_clipboard_read_bytes_finish:
 * @clipboard: a #ValentClipboard
 * @result: a #GAsyncResult
 * @error: (nullable): a #GError
 *
 * Finish an operation started by [method@Valent.Clipboard.read_bytes].
 *
 * Returns: (transfer full) (nullable): the content
 *
 * Since: 1.0
 */
GBytes *
valent_clipboard_read_bytes_finish (ValentClipboard  *clipboard,
                                    GAsyncResult     *result,
                                    GError          **error)
{
  GBytes *ret;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_CLIPBOARD (clipboard), NULL);
  g_return_val_if_fail (g_task_is_valid (result, clipboard), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  ret = g_task_propagate_pointer (G_TASK (result), error);

  VALENT_RETURN (g_steal_pointer (&ret));
}

/**
 * valent_clipboard_write_bytes:
 * @clipboard: a #ValentClipboard
 * @mimetype: (nullable): a mime-type, or %NULL if @bytes is %NULL
 * @bytes: (nullable): a #GBytes, or %NULL if @mimetype is %NULL
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): a #GAsyncReadyCallback
 * @user_data: (closure): user supplied data
 *
 * Set the content of the primary clipboard adapter.
 *
 * Call [method@Valent.Clipboard.write_bytes_finish] to get the result.
 *
 * Since: 1.0
 */
void
valent_clipboard_write_bytes (ValentClipboard     *clipboard,
                              const char          *mimetype,
                              GBytes              *bytes,
                              GCancellable        *cancellable,
                              GAsyncReadyCallback  callback,
                              gpointer             user_data)
{
  g_autoptr (GTask) task = NULL;

  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_CLIPBOARD (clipboard));
  g_return_if_fail (bytes == NULL || (mimetype != NULL && *mimetype != '\0'));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  if G_UNLIKELY (clipboard->default_adapter == NULL)
    {
      g_task_report_new_error (clipboard, callback, user_data,
                               valent_clipboard_write_bytes,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_SUPPORTED,
                               "No clipboard adapter available");
      return;
    }

  task = g_task_new (clipboard, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_clipboard_write_bytes);
  valent_clipboard_adapter_write_bytes (clipboard->default_adapter,
                                        mimetype,
                                        bytes,
                                        cancellable,
                                        (GAsyncReadyCallback)valent_clipboard_adapter_write_bytes_cb,
                                        g_steal_pointer (&task));

  VALENT_EXIT;
}

/**
 * valent_clipboard_write_bytes_finish:
 * @clipboard: a #ValentClipboard
 * @result: a #GAsyncResult
 * @error: (nullable): a #GError
 *
 * Finish an operation started by [method@Valent.Clipboard.write_bytes].
 *
 * Returns: %TRUE if successful, or %FALSE with @error set
 *
 * Since: 1.0
 */
gboolean
valent_clipboard_write_bytes_finish (ValentClipboard  *clipboard,
                                     GAsyncResult     *result,
                                     GError          **error)
{
  gboolean ret;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_CLIPBOARD (clipboard), FALSE);
  g_return_val_if_fail (g_task_is_valid (result, clipboard), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  ret = g_task_propagate_boolean (G_TASK (result), error);

  VALENT_RETURN (ret);
}

/**
 * valent_clipboard_read_text:
 * @clipboard: a #ValentClipboard
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): a #GAsyncReadyCallback
 * @user_data: (closure): user supplied data
 *
 * Get the text content of the primary clipboard adapter.
 *
 * Call [method@Valent.Clipboard.read_text_finish] to get the result.
 *
 * Since: 1.0
 */
void
valent_clipboard_read_text (ValentClipboard     *clipboard,
                            GCancellable        *cancellable,
                            GAsyncReadyCallback  callback,
                            gpointer             user_data)
{
  g_autoptr (GTask) task = NULL;

  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_CLIPBOARD (clipboard));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  if G_UNLIKELY (clipboard->default_adapter == NULL)
    {
      g_task_report_new_error (clipboard, callback, user_data,
                               valent_clipboard_read_text,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_SUPPORTED,
                               "No clipboard adapter available");
      return;
    }

  task = g_task_new (clipboard, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_clipboard_read_text);
  valent_clipboard_adapter_read_text (clipboard->default_adapter,
                                      cancellable,
                                      (GAsyncReadyCallback)valent_clipboard_adapter_read_text_cb,
                                      g_steal_pointer (&task));

  VALENT_EXIT;
}

/**
 * valent_clipboard_read_text_finish:
 * @clipboard: a #ValentClipboard
 * @result: a #GAsyncResult
 * @error: (nullable): a #GError
 *
 * Finish an operation started by [method@Valent.Clipboard.read_text].
 *
 * Returns: (transfer full) (nullable): the text content
 *
 * Since: 1.0
 */
char *
valent_clipboard_read_text_finish (ValentClipboard  *clipboard,
                                   GAsyncResult     *result,
                                   GError          **error)
{
  char *ret;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_CLIPBOARD (clipboard), NULL);
  g_return_val_if_fail (g_task_is_valid (result, clipboard), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  ret = g_task_propagate_pointer (G_TASK (result), error);

  VALENT_RETURN (g_steal_pointer (&ret));
}

/**
 * valent_clipboard_write_text:
 * @clipboard: a #ValentClipboard
 * @text: (nullable): text content
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): a #GAsyncReadyCallback
 * @user_data: (closure): user supplied data
 *
 * Set the text content of the primary clipboard adapter.
 *
 * Call [method@Valent.Clipboard.write_text_finish] to get the result.
 *
 * Since: 1.0
 */
void
valent_clipboard_write_text (ValentClipboard     *clipboard,
                             const char          *text,
                             GCancellable        *cancellable,
                             GAsyncReadyCallback  callback,
                             gpointer             user_data)
{
  g_autoptr (GTask) task = NULL;

  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_CLIPBOARD (clipboard));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  if G_UNLIKELY (clipboard->default_adapter == NULL)
    {
      g_task_report_new_error (clipboard, callback, user_data,
                               valent_clipboard_write_text,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_SUPPORTED,
                               "No clipboard adapter available");
      return;
    }

  task = g_task_new (clipboard, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_clipboard_write_text);
  valent_clipboard_adapter_write_text (clipboard->default_adapter,
                                       text,
                                       cancellable,
                                       (GAsyncReadyCallback)valent_clipboard_adapter_write_text_cb,
                                       g_steal_pointer (&task));

  VALENT_EXIT;
}

/**
 * valent_clipboard_write_text_finish:
 * @clipboard: a #ValentClipboard
 * @result: a #GAsyncResult
 * @error: (nullable): a #GError
 *
 * Finish an operation started by [method@Valent.Clipboard.write_text].
 *
 * Returns: %TRUE if successful, or %FALSE with @error set
 *
 * Since: 1.0
 */
gboolean
valent_clipboard_write_text_finish (ValentClipboard  *clipboard,
                                    GAsyncResult     *result,
                                    GError          **error)
{
  gboolean ret;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_CLIPBOARD (clipboard), FALSE);
  g_return_val_if_fail (g_task_is_valid (result, clipboard), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  ret = g_task_propagate_boolean (G_TASK (result), error);

  VALENT_RETURN (ret);
}

