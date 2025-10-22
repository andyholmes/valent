// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-mock-clipboard"

#include "config.h"

#include <libpeas.h>
#include <valent.h>

#include "valent-mock-clipboard-adapter.h"


struct _ValentMockClipboardAdapter
{
  ValentClipboardAdapter  parent_instance;

  GBytes                 *content;
  GStrv                   mimetypes;
  int64_t                 timestamp;
};

G_DEFINE_FINAL_TYPE (ValentMockClipboardAdapter, valent_mock_clipboard_adapter, VALENT_TYPE_CLIPBOARD_ADAPTER)


static gboolean
task_source_cb (gpointer data)
{
  GTask *task = G_TASK (data);
  ValentMockClipboardAdapter *self = g_task_get_source_object (task);
  GBytes *bytes = g_task_get_task_data (task);
  const char *mimetype = g_object_get_data (G_OBJECT (task), "mock-mimetype");

  g_clear_pointer (&self->content, g_bytes_unref);
  g_clear_pointer (&self->mimetypes, g_strfreev);

  self->content = g_bytes_ref (bytes);
  self->mimetypes = g_strdupv ((char *[]){(char *)mimetype, NULL});
  self->timestamp = valent_timestamp_ms ();

  valent_clipboard_adapter_changed (VALENT_CLIPBOARD_ADAPTER (self));
  g_task_return_boolean (task, TRUE);

  return G_SOURCE_REMOVE;
}

/*
 * ValentClipboardAdapter
 */
static GStrv
valent_mock_clipboard_adapter_get_mimetypes (ValentClipboardAdapter *adapter)
{
  ValentMockClipboardAdapter *self = VALENT_MOCK_CLIPBOARD_ADAPTER (adapter);

  g_assert (VALENT_IS_MOCK_CLIPBOARD_ADAPTER (self));

  return g_strdupv (self->mimetypes);
}

static int64_t
valent_mock_clipboard_adapter_get_timestamp (ValentClipboardAdapter *adapter)
{
  ValentMockClipboardAdapter *self = VALENT_MOCK_CLIPBOARD_ADAPTER (adapter);

  g_assert (VALENT_IS_MOCK_CLIPBOARD_ADAPTER (self));

  return self->timestamp;
}

static void
valent_mock_clipboard_adapter_read_bytes (ValentClipboardAdapter *adapter,
                                          const char             *mimetype,
                                          GCancellable           *cancellable,
                                          GAsyncReadyCallback     callback,
                                          gpointer                user_data)
{
  ValentMockClipboardAdapter *self = VALENT_MOCK_CLIPBOARD_ADAPTER (adapter);
  g_autoptr (GTask) task = NULL;

  g_assert (VALENT_IS_MOCK_CLIPBOARD_ADAPTER (self));
  g_assert (mimetype != NULL && *mimetype != '\0');
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  if (self->content == NULL)
    {
      g_task_report_new_error (adapter, callback, user_data,
                               valent_mock_clipboard_adapter_read_bytes,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_FOUND,
                               "Clipboard empty");
      return;
    }

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_mock_clipboard_adapter_read_bytes);
  g_task_return_pointer (task,
                         g_bytes_ref (self->content),
                         (GDestroyNotify)g_bytes_unref);
}

static void
valent_mock_clipboard_adapter_write_bytes (ValentClipboardAdapter *adapter,
                                           const char             *mimetype,
                                           GBytes                 *bytes,
                                           GCancellable           *cancellable,
                                           GAsyncReadyCallback     callback,
                                           gpointer                user_data)
{
  ValentMockClipboardAdapter *self = VALENT_MOCK_CLIPBOARD_ADAPTER (adapter);
  g_autoptr (GTask) task = NULL;
  g_autoptr (GSource) source = NULL;

  g_assert (VALENT_IS_MOCK_CLIPBOARD_ADAPTER (self));
  g_assert (bytes == NULL || (mimetype != NULL && *mimetype != '\0'));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_task_data (task, g_bytes_ref (bytes), (GDestroyNotify)g_bytes_unref);
  g_object_set_data_full (G_OBJECT (task), "mock-mimetype", g_strdup (mimetype), g_free);
  g_task_set_source_tag (task, valent_mock_clipboard_adapter_write_bytes);

  if (g_bytes_equal (self->content, bytes))
    {
      g_task_return_boolean (task, TRUE);
      return;
    }

  source = g_idle_source_new ();
  g_task_attach_source(task, source, task_source_cb);
}

/*
 * GObject
 */
static void
valent_mock_clipboard_adapter_finalize (GObject *object)
{
  ValentMockClipboardAdapter *self = VALENT_MOCK_CLIPBOARD_ADAPTER (object);

  g_clear_pointer (&self->content, g_bytes_unref);
  g_clear_pointer (&self->mimetypes, g_strfreev);

  G_OBJECT_CLASS (valent_mock_clipboard_adapter_parent_class)->finalize (object);
}

static void
valent_mock_clipboard_adapter_class_init (ValentMockClipboardAdapterClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentClipboardAdapterClass *clipboard_class = VALENT_CLIPBOARD_ADAPTER_CLASS (klass);

  object_class->finalize = valent_mock_clipboard_adapter_finalize;

  clipboard_class->get_mimetypes = valent_mock_clipboard_adapter_get_mimetypes;
  clipboard_class->get_timestamp = valent_mock_clipboard_adapter_get_timestamp;
  clipboard_class->read_bytes = valent_mock_clipboard_adapter_read_bytes;
  clipboard_class->write_bytes = valent_mock_clipboard_adapter_write_bytes;
}

static void
valent_mock_clipboard_adapter_init (ValentMockClipboardAdapter *self)
{
  self->content = g_bytes_new ("connect", strlen ("connect") + 1);
  self->mimetypes = g_strdupv ((char *[]){"text/plain;charset=utf-8", NULL});
}

