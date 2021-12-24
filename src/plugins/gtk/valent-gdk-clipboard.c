// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-gdk-clipboard"

#include "config.h"

#include <gdk/gdk.h>
#include <libpeas/peas.h>
#include <libvalent-core.h>
#include <libvalent-clipboard.h>

#include "valent-gdk-clipboard.h"


struct _ValentGdkClipboard
{
  ValentClipboardAdapter  parent_instance;

  GCancellable           *cancellable;

  GdkClipboard           *clipboard;
  unsigned long           changed_id;
};

G_DEFINE_TYPE (ValentGdkClipboard, valent_gdk_clipboard, VALENT_TYPE_CLIPBOARD_ADAPTER)


/*
 * ValentClipboardAdapter
 */
static void
get_text_cb (GdkClipboard *clipboard,
             GAsyncResult *result,
             gpointer      user_data)
{
  g_autoptr (GTask) task = G_TASK (user_data);
  g_autofree char *text = NULL;
  GError *error = NULL;

  g_assert (GDK_IS_CLIPBOARD (clipboard));
  g_assert (g_task_is_valid (result, clipboard));

  text = gdk_clipboard_read_text_finish (clipboard, result, &error);

  if (text == NULL)
    return g_task_return_error (task, error);

  g_task_return_pointer (task, g_steal_pointer (&text), g_free);
}

static void
valent_gdk_clipboard_get_text_async (ValentClipboardAdapter *adapter,
                                     GCancellable           *cancellable,
                                     GAsyncReadyCallback     callback,
                                     gpointer                user_data)
{
  ValentGdkClipboard *self = VALENT_GDK_CLIPBOARD (adapter);
  g_autoptr (GTask) task = NULL;

  g_assert (VALENT_IS_GDK_CLIPBOARD (self));
  g_assert (GDK_IS_CLIPBOARD (self->clipboard));

  if (cancellable != NULL)
    g_signal_connect_object (cancellable,
                             "cancelled",
                             G_CALLBACK (g_cancellable_cancel),
                             self->cancellable,
                             G_CONNECT_SWAPPED);

  task = g_task_new (adapter, self->cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_gdk_clipboard_get_text_async);

  gdk_clipboard_read_text_async (self->clipboard,
                                 self->cancellable,
                                 (GAsyncReadyCallback)get_text_cb,
                                 g_steal_pointer (&task));
}

static void
valent_gdk_clipboard_set_text (ValentClipboardAdapter *adapter,
                               const char             *text)
{
  ValentGdkClipboard *self = VALENT_GDK_CLIPBOARD (adapter);

  g_assert (VALENT_IS_GDK_CLIPBOARD (self));
  g_assert (GDK_IS_CLIPBOARD (self->clipboard));

  gdk_clipboard_set (self->clipboard, G_TYPE_STRING, text);
}

static gint64
valent_gdk_clipboard_get_timestamp (ValentClipboardAdapter *adapter)
{
  ValentGdkClipboard *self = VALENT_GDK_CLIPBOARD (adapter);

  g_assert (VALENT_IS_GDK_CLIPBOARD (self));
  g_assert (GDK_IS_CLIPBOARD (self->clipboard));

  return 0;
}


/*
 * GdkClipboard Callbacks
 */
static void
on_changed (GdkClipboard *clipboard,
            gpointer      user_data)
{
  ValentClipboardAdapter *adapter = VALENT_CLIPBOARD_ADAPTER (user_data);

  g_assert (VALENT_IS_CLIPBOARD_ADAPTER (adapter));
  g_assert (VALENT_IS_GDK_CLIPBOARD (user_data));

  valent_clipboard_adapter_emit_changed (adapter);
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
      self->changed_id = g_signal_connect (self->clipboard,
                                           "changed",
                                           G_CALLBACK (on_changed),
                                           self);
    }

  G_OBJECT_CLASS (valent_gdk_clipboard_parent_class)->constructed (object);
}

static void
valent_gdk_clipboard_dispose (GObject *object)
{
  ValentGdkClipboard *self = VALENT_GDK_CLIPBOARD (object);

  if (!g_cancellable_is_cancelled (self->cancellable))
    g_cancellable_cancel (self->cancellable);

  if (self->clipboard)
    g_clear_signal_handler (&self->changed_id, self->clipboard);

  G_OBJECT_CLASS (valent_gdk_clipboard_parent_class)->dispose (object);
}

static void
valent_gdk_clipboard_finalize (GObject *object)
{
  ValentGdkClipboard *self = VALENT_GDK_CLIPBOARD (object);

  g_clear_object (&self->cancellable);
  self->clipboard = NULL;

  G_OBJECT_CLASS (valent_gdk_clipboard_parent_class)->finalize (object);
}

static void
valent_gdk_clipboard_class_init (ValentGdkClipboardClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentClipboardAdapterClass *clipboard_class = VALENT_CLIPBOARD_ADAPTER_CLASS (klass);

  object_class->constructed = valent_gdk_clipboard_constructed;
  object_class->dispose = valent_gdk_clipboard_dispose;
  object_class->finalize = valent_gdk_clipboard_finalize;

  clipboard_class->get_text_async = valent_gdk_clipboard_get_text_async;
  clipboard_class->set_text = valent_gdk_clipboard_set_text;
}

static void
valent_gdk_clipboard_init (ValentGdkClipboard *self)
{
  self->cancellable = g_cancellable_new ();
  self->clipboard = NULL;
  self->changed_id = 0;
}

