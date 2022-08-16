// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-mock-clipboard"

#include "config.h"

#include <libpeas/peas.h>
#include <libvalent-core.h>
#include <libvalent-clipboard.h>

#include "valent-mock-clipboard-adapter.h"


struct _ValentMockClipboardAdapter
{
  ValentClipboardAdapter  parent_instance;

  char                   *text;
  GStrv                   mimetypes;
  gint64                  timestamp;
};

G_DEFINE_TYPE (ValentMockClipboardAdapter, valent_mock_clipboard_adapter, VALENT_TYPE_CLIPBOARD_ADAPTER)


static ValentClipboardAdapter *test_instance = NULL;

/*
 * ValentClipboardAdapter
 */
static void
valent_mock_clipboard_adapter_get_bytes (ValentClipboardAdapter *adapter,
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

  if (self->text == NULL)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_FOUND,
                               "Clipboard empty");
      return;
    }

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_mock_clipboard_adapter_get_bytes);
  g_task_return_pointer (task,
                         g_bytes_new (self->text, strlen (self->text) + 1),
                         (GDestroyNotify)g_bytes_unref);
}

static void
valent_mock_clipboard_adapter_set_bytes (ValentClipboardAdapter *adapter,
                                         const char             *mimetype,
                                         GBytes                 *bytes)
{
  ValentMockClipboardAdapter *self = VALENT_MOCK_CLIPBOARD_ADAPTER (adapter);
  const char *text = NULL;

  g_assert (VALENT_IS_MOCK_CLIPBOARD_ADAPTER (self));
  g_assert (bytes == NULL || (mimetype != NULL && *mimetype != '\0'));

  if (bytes != NULL && g_str_has_prefix (mimetype, "text/plain"))
    text = g_bytes_get_data (bytes, NULL);

  if (g_strcmp0 (self->text, text) == 0)
    return;

  g_clear_pointer (&self->mimetypes, g_strfreev);
  g_clear_pointer (&self->text, g_free);

  self->mimetypes = g_strdupv ((char *[]){"text/plain;charset=utf-8", NULL});
  self->text = g_strdup (text);
  self->timestamp = valent_timestamp_ms ();

  valent_clipboard_adapter_emit_changed (adapter);
}

static void
valent_mock_clipboard_adapter_get_text_async (ValentClipboardAdapter *adapter,
                                              GCancellable           *cancellable,
                                              GAsyncReadyCallback     callback,
                                              gpointer                user_data)
{
  ValentMockClipboardAdapter *self = VALENT_MOCK_CLIPBOARD_ADAPTER (adapter);
  g_autoptr (GTask) task = NULL;

  g_assert (VALENT_IS_MOCK_CLIPBOARD_ADAPTER (self));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_return_pointer (task, g_strdup (self->text), g_free);
}

static void
valent_mock_clipboard_adapter_set_text (ValentClipboardAdapter *adapter,
                                        const char             *text)
{
  ValentMockClipboardAdapter *self = VALENT_MOCK_CLIPBOARD_ADAPTER (adapter);

  g_assert (VALENT_IS_MOCK_CLIPBOARD_ADAPTER (self));

  if (g_strcmp0 (self->text, text) == 0)
    return;

  g_clear_pointer (&self->mimetypes, g_strfreev);
  g_clear_pointer (&self->text, g_free);

  self->mimetypes = g_strdupv ((char *[]){"text/plain;charset=utf-8", NULL});
  self->text = g_strdup (text);
  self->timestamp = valent_timestamp_ms ();

  valent_clipboard_adapter_emit_changed (adapter);
}

static char **
valent_mock_clipboard_adapter_get_mimetypes (ValentClipboardAdapter *adapter)
{
  ValentMockClipboardAdapter *self = VALENT_MOCK_CLIPBOARD_ADAPTER (adapter);

  g_assert (VALENT_IS_MOCK_CLIPBOARD_ADAPTER (self));

  return g_strdupv (self->mimetypes);
}

static gint64
valent_mock_clipboard_adapter_get_timestamp (ValentClipboardAdapter *adapter)
{
  ValentMockClipboardAdapter *self = VALENT_MOCK_CLIPBOARD_ADAPTER (adapter);

  g_assert (VALENT_IS_MOCK_CLIPBOARD_ADAPTER (self));

  return self->timestamp;
}

/*
 * GObject
 */
static void
valent_mock_clipboard_adapter_finalize (GObject *object)
{
  ValentMockClipboardAdapter *self = VALENT_MOCK_CLIPBOARD_ADAPTER (object);

  g_clear_pointer (&self->mimetypes, g_strfreev);
  g_clear_pointer (&self->text, g_free);

  G_OBJECT_CLASS (valent_mock_clipboard_adapter_parent_class)->finalize (object);
}

static void
valent_mock_clipboard_adapter_class_init (ValentMockClipboardAdapterClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentClipboardAdapterClass *clipboard_class = VALENT_CLIPBOARD_ADAPTER_CLASS (klass);

  object_class->finalize = valent_mock_clipboard_adapter_finalize;

  clipboard_class->get_bytes = valent_mock_clipboard_adapter_get_bytes;
  clipboard_class->set_bytes = valent_mock_clipboard_adapter_set_bytes;
  clipboard_class->get_mimetypes = valent_mock_clipboard_adapter_get_mimetypes;
  clipboard_class->get_text_async = valent_mock_clipboard_adapter_get_text_async;
  clipboard_class->set_text = valent_mock_clipboard_adapter_set_text;
  clipboard_class->get_timestamp = valent_mock_clipboard_adapter_get_timestamp;
}

static void
valent_mock_clipboard_adapter_init (ValentMockClipboardAdapter *self)
{
  self->mimetypes = g_strdupv ((char *[]){"text/plain;charset=utf-8", NULL});
  self->text = g_strdup ("connect");

  if (test_instance == NULL)
    {
      test_instance = VALENT_CLIPBOARD_ADAPTER (self);
      g_object_add_weak_pointer (G_OBJECT (test_instance),
                                 (gpointer)&test_instance);
    }
}

/**
 * valent_mock_clipboard_adapter_get_instance:
 *
 * Get the #ValentMockClipboardAdapter instance.
 *
 * Returns: (transfer none) (nullable): a #ValentClipboardAdapter
 */
ValentClipboardAdapter *
valent_mock_clipboard_adapter_get_instance (void)
{
  return test_instance;
}

