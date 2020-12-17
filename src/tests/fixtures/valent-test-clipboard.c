// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-test-clipboard"

#include "config.h"

#include <libpeas/peas.h>
#include <libvalent-core.h>
#include <libvalent-clipboard.h>

#include "valent-test-clipboard.h"


struct _ValentTestClipboard
{
  ValentClipboardSource  parent_instance;

  char                  *text;
};

G_DEFINE_TYPE (ValentTestClipboard, valent_test_clipboard, VALENT_TYPE_CLIPBOARD_SOURCE)


/*
 * ValentClipboardSource
 */
static void
valent_test_clipboard_get_text_async (ValentClipboardSource *source,
                                      GCancellable          *cancellable,
                                      GAsyncReadyCallback    callback,
                                      gpointer               user_data)
{
  ValentTestClipboard *self = VALENT_TEST_CLIPBOARD (source);
  g_autoptr (GTask) task = NULL;

  g_assert (VALENT_IS_TEST_CLIPBOARD (self));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_return_pointer (task, g_strdup (self->text), g_free);
}

static void
valent_test_clipboard_set_text (ValentClipboardSource *source,
                                const char            *text)
{
  ValentTestClipboard *self = VALENT_TEST_CLIPBOARD (source);

  g_assert (VALENT_IS_TEST_CLIPBOARD (self));

  if (g_strcmp0 (self->text, text) == 0)
    return;

  g_clear_pointer (&self->text, g_free);
  self->text = g_strdup (text);
  valent_clipboard_source_emit_changed (source);
}

/*
 * GObject
 */
static void
valent_test_clipboard_finalize (GObject *object)
{
  ValentTestClipboard *self = VALENT_TEST_CLIPBOARD (object);

  g_clear_pointer (&self->text, g_free);

  G_OBJECT_CLASS (valent_test_clipboard_parent_class)->finalize (object);
}

static void
valent_test_clipboard_class_init (ValentTestClipboardClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentClipboardSourceClass *clipboard_class = VALENT_CLIPBOARD_SOURCE_CLASS (klass);

  object_class->finalize = valent_test_clipboard_finalize;

  clipboard_class->get_text_async = valent_test_clipboard_get_text_async;
  clipboard_class->set_text = valent_test_clipboard_set_text;
}

static void
valent_test_clipboard_init (ValentTestClipboard *self)
{
  self->text = g_strdup ("connect");
}

