// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-gdk-clipboard"

#include "config.h"

#include <gdk/gdk.h>
#include <valent.h>

#include "valent-gdk-clipboard.h"

#define CLIPBOARD_MAXSIZE (16 * 1024)


struct _ValentGdkClipboard
{
  ValentClipboardAdapter  parent_instance;

  GdkClipboard           *clipboard;
  int64_t                 timestamp;
};

G_DEFINE_FINAL_TYPE (ValentGdkClipboard, valent_gdk_clipboard, VALENT_TYPE_CLIPBOARD_ADAPTER)

static const char * const text_mimetypes[] = {
  "text/plain;charset=utf-8",
  "text/plain",
  "UTF8_STRING",
  "STRING",
  "TEXT",
  "COMPOUND_TEXT",
  NULL,
};


/*
 * GdkClipboard Callbacks
 */
static void
on_changed (GdkClipboard       *clipboard,
            ValentGdkClipboard *self)
{
  ValentClipboardAdapter *adapter = VALENT_CLIPBOARD_ADAPTER (self);

  g_assert (VALENT_IS_GDK_CLIPBOARD (self));
  g_assert (VALENT_IS_CLIPBOARD_ADAPTER (adapter));

  // TODO: get the actual TIMESTAMP value
  self->timestamp = valent_timestamp_ms ();
  valent_clipboard_adapter_changed (adapter);
}

static void
g_input_stream_read_bytes_cb (GInputStream *stream,
                              GAsyncResult *result,
                              gpointer      user_data)
{
  g_autoptr (GTask) task = G_TASK (user_data);
  g_autoptr (GBytes) bytes = NULL;
  g_autoptr (GError) error = NULL;

  bytes = g_input_stream_read_bytes_finish (stream, result, &error);

  if (bytes == NULL)
    return g_task_return_error (task, g_steal_pointer (&error));

  g_task_return_pointer (task,
                         g_bytes_ref (bytes),
                         (GDestroyNotify)g_bytes_unref);
}

static void
gdk_clipboard_read_cb (GdkClipboard *clipboard,
                       GAsyncResult *result,
                       gpointer      user_data)
{
  g_autoptr (GTask) task = G_TASK (user_data);
  GCancellable *cancellable = g_task_get_cancellable (task);
  g_autoptr (GInputStream) input = NULL;
  const char *mimetype = NULL;
  g_autoptr (GError) error = NULL;

  g_assert (GDK_IS_CLIPBOARD (clipboard));
  g_assert (g_task_is_valid (result, clipboard));

  input = gdk_clipboard_read_finish (clipboard, result, &mimetype, &error);

  if (input == NULL)
    return g_task_return_error (task, g_steal_pointer (&error));

  g_input_stream_read_bytes_async (input,
                                   G_PRIORITY_DEFAULT,
                                   CLIPBOARD_MAXSIZE,
                                   cancellable,
                                   (GAsyncReadyCallback)g_input_stream_read_bytes_cb,
                                   g_steal_pointer (&task));
}

static void
gdk_clipboard_read_text_cb (GdkClipboard *clipboard,
                            GAsyncResult *result,
                            gpointer      user_data)
{
  g_autoptr (GTask) task = G_TASK (user_data);
  char *text = NULL;
  GError *error = NULL;

  g_assert (GDK_IS_CLIPBOARD (clipboard));
  g_assert (g_task_is_valid (result, clipboard));

  text = gdk_clipboard_read_text_finish (clipboard, result, &error);

  if (text == NULL)
    return g_task_return_error (task, error);

  g_task_return_pointer (task,
                         g_bytes_new_take (text, strlen (text) + 1),
                         (GDestroyNotify)g_bytes_unref);
}

/*
 * ValentClipboardAdapter
 */
static GStrv
valent_gdk_clipboard_get_mimetypes (ValentClipboardAdapter *adapter)
{
  ValentGdkClipboard *self = VALENT_GDK_CLIPBOARD (adapter);
  GdkContentFormats *formats = NULL;
  const char * const *mimetypes = NULL;

  g_assert (VALENT_IS_GDK_CLIPBOARD (self));
  g_return_val_if_fail (GDK_IS_CLIPBOARD (self->clipboard), NULL);

  if ((formats = gdk_clipboard_get_formats (self->clipboard)) == NULL)
    return NULL;

  mimetypes = gdk_content_formats_get_mime_types (formats, NULL);

  return g_strdupv ((char **)mimetypes);
}

static int64_t
valent_gdk_clipboard_get_timestamp (ValentClipboardAdapter *adapter)
{
  ValentGdkClipboard *self = VALENT_GDK_CLIPBOARD (adapter);

  g_assert (VALENT_IS_GDK_CLIPBOARD (self));
  g_return_val_if_fail (GDK_IS_CLIPBOARD (self->clipboard), 0);

  return self->timestamp;
}

static void
valent_gdk_clipboard_read_bytes (ValentClipboardAdapter *adapter,
                                 const char             *mimetype,
                                 GCancellable           *cancellable,
                                 GAsyncReadyCallback     callback,
                                 gpointer                user_data)
{
  ValentGdkClipboard *self = VALENT_GDK_CLIPBOARD (adapter);
  g_autoptr (GTask) task = NULL;
  GdkContentFormats *formats = NULL;

  g_assert (VALENT_IS_GDK_CLIPBOARD (self));
  g_assert (mimetype != NULL && *mimetype != '\0');

  if G_UNLIKELY (!GDK_IS_CLIPBOARD (self->clipboard))
    {
      g_task_report_new_error (adapter, callback, user_data,
                               valent_gdk_clipboard_read_bytes,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_SUPPORTED,
                               "Clipboard not available");
      return;
    }

  /* Special case for text */
  if (g_strv_contains (text_mimetypes, mimetype))
    {
      task = g_task_new (adapter, cancellable, callback, user_data);
      g_task_set_source_tag (task, valent_gdk_clipboard_read_bytes);

      gdk_clipboard_read_text_async (self->clipboard,
                                     cancellable,
                                     (GAsyncReadyCallback)gdk_clipboard_read_text_cb,
                                     g_steal_pointer (&task));
      return;
    }

  if ((formats = gdk_clipboard_get_formats (self->clipboard)) == NULL)
    {
      g_task_report_new_error (adapter, callback, user_data,
                               valent_gdk_clipboard_read_bytes,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_SUPPORTED,
                               "Clipboard empty");
      return;
    }

  if (!gdk_content_formats_contain_mime_type (formats, mimetype))
    {
      g_task_report_new_error (adapter, callback, user_data,
                               valent_gdk_clipboard_read_bytes,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_SUPPORTED,
                               "%s format not available.",
                               mimetype);
      return;
    }

  task = g_task_new (adapter, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_gdk_clipboard_read_bytes);
  g_task_set_task_data (task, g_strdup (mimetype), g_free);

  gdk_clipboard_read_async (self->clipboard,
                            (const char *[]){ mimetype, NULL },
                            G_PRIORITY_DEFAULT,
                            cancellable,
                            (GAsyncReadyCallback)gdk_clipboard_read_cb,
                            g_steal_pointer (&task));
}

static void
valent_gdk_clipboard_write_bytes (ValentClipboardAdapter *adapter,
                                  const char             *mimetype,
                                  GBytes                 *bytes,
                                  GCancellable           *cancellable,
                                  GAsyncReadyCallback     callback,
                                  gpointer                user_data)
{
  ValentGdkClipboard *self = VALENT_GDK_CLIPBOARD (adapter);
  g_autoptr (GdkContentProvider) content = NULL;
  g_autoptr (GTask) task = NULL;

  g_assert (VALENT_IS_GDK_CLIPBOARD (self));
  g_assert (bytes != NULL || (mimetype != NULL && *mimetype != '\0'));
  g_return_if_fail (GDK_IS_CLIPBOARD (self->clipboard));

  if (bytes != NULL)
    content = gdk_content_provider_new_for_bytes (mimetype, bytes);

  if (!gdk_clipboard_set_content (self->clipboard, content))
    {
      g_task_report_new_error (adapter, callback, user_data,
                               valent_gdk_clipboard_write_bytes,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               "Failed to set clipboard content");
      return;
    }

  task = g_task_new (adapter, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_gdk_clipboard_write_bytes);
  g_task_return_boolean (task, TRUE);
}

/*
 * GObject
 */
static void
valent_gdk_clipboard_constructed (GObject *object)
{
  ValentGdkClipboard *self = VALENT_GDK_CLIPBOARD (object);
  GdkDisplay *display;

  /* Connect to the clipboard */
  if ((display = gdk_display_get_default ()) != NULL)
    {
      self->clipboard = gdk_display_get_clipboard (display);
      g_signal_connect_object (self->clipboard,
                               "changed",
                               G_CALLBACK (on_changed),
                               self, 0);
    }

  G_OBJECT_CLASS (valent_gdk_clipboard_parent_class)->constructed (object);
}

static void
valent_gdk_clipboard_dispose (GObject *object)
{
  ValentGdkClipboard *self = VALENT_GDK_CLIPBOARD (object);

  if (self->clipboard != NULL)
    {
      g_signal_handlers_disconnect_by_data (self->clipboard, self);
      self->clipboard = NULL;
    }

  G_OBJECT_CLASS (valent_gdk_clipboard_parent_class)->dispose (object);
}

static void
valent_gdk_clipboard_class_init (ValentGdkClipboardClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentClipboardAdapterClass *clipboard_class = VALENT_CLIPBOARD_ADAPTER_CLASS (klass);

  object_class->constructed = valent_gdk_clipboard_constructed;
  object_class->dispose = valent_gdk_clipboard_dispose;

  clipboard_class->get_mimetypes = valent_gdk_clipboard_get_mimetypes;
  clipboard_class->get_timestamp = valent_gdk_clipboard_get_timestamp;
  clipboard_class->read_bytes = valent_gdk_clipboard_read_bytes;
  clipboard_class->write_bytes = valent_gdk_clipboard_write_bytes;
}

static void
valent_gdk_clipboard_init (ValentGdkClipboard *self)
{
}

