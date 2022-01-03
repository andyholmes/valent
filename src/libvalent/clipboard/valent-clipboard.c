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
 * SECTION:valentclipboard
 * @short_description: Clipboard Abstraction
 * @title: ValentClipboard
 * @stability: Unstable
 * @include: libvalent-clipboard.h
 *
 * #ValentClipboard is an abstraction of the available #ValentClipboardAdapter
 * implementations, generally intended to be used by #ValentDevicePlugin
 * implementations.
 *
 * Plugins can provide implementations by subclassing the
 * #ValentClipboardAdapter base class. The priority of implementations is
 * determined by the `.plugin` file key `X-ClipboardAdapterPriority`, with the
 * lowest value taking precedence.
 */

struct _ValentClipboard
{
  ValentComponent         parent_instance;

  GCancellable           *cancellable;
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
get_text_cb (ValentClipboardAdapter *adapter,
             GAsyncResult           *result,
             gpointer                user_data)
{
  g_autoptr (GTask) task = G_TASK (user_data);
  g_autoptr (GError) error = NULL;
  g_autofree char *text = NULL;

  g_assert (VALENT_IS_CLIPBOARD_ADAPTER (adapter));
  g_assert (g_task_is_valid (result, adapter));

  text = valent_clipboard_adapter_get_text_finish (adapter, result, &error);

  if (text == NULL)
    return g_task_return_error (task, g_steal_pointer (&error));

  g_task_return_pointer (task, g_steal_pointer (&text), g_free);
}

static void
on_clipboard_adapter_changed (ValentClipboardAdapter *clipboard,
                              ValentClipboard        *self)
{
  VALENT_ENTRY;

  if (clipboard == self->default_adapter)
    g_signal_emit (G_OBJECT (self), signals [CHANGED], 0);

  VALENT_EXIT;
}

/*
 * ValentComponent
 */
static void
valent_clipboard_extension_added (ValentComponent *component,
                                  PeasExtension   *extension)
{
  ValentClipboard *self = VALENT_CLIPBOARD (component);
  ValentClipboardAdapter *clipboard = VALENT_CLIPBOARD_ADAPTER (extension);
  PeasExtension *provider;

  VALENT_ENTRY;

  g_assert (VALENT_IS_CLIPBOARD (self));

  g_signal_connect_object (clipboard,
                           "changed",
                           G_CALLBACK (on_clipboard_adapter_changed),
                           component, 0);

  provider = valent_component_get_priority_provider (component,
                                                     "ClipboardAdapterPriority");

  if ((PeasExtension *)self->default_adapter != provider)
    g_set_object (&self->default_adapter, VALENT_CLIPBOARD_ADAPTER (provider));

  VALENT_EXIT;
}

static void
valent_clipboard_extension_removed (ValentComponent *component,
                                    PeasExtension   *extension)
{
  ValentClipboard *self = VALENT_CLIPBOARD (component);
  ValentClipboardAdapter *clipboard = VALENT_CLIPBOARD_ADAPTER (extension);
  PeasExtension *provider;

  VALENT_ENTRY;

  g_assert (VALENT_IS_CLIPBOARD (self));

  g_signal_handlers_disconnect_by_data (clipboard, self);

  provider = valent_component_get_priority_provider (component,
                                                     "ClipboardAdapterPriority");
  g_set_object (&self->default_adapter, VALENT_CLIPBOARD_ADAPTER (provider));

  VALENT_EXIT;
}

/*
 * GObject
 */
static void
valent_clipboard_dispose (GObject *object)
{
  ValentClipboard *self = VALENT_CLIPBOARD (object);

  if (!g_cancellable_is_cancelled (self->cancellable))
    g_cancellable_cancel (self->cancellable);

  G_OBJECT_CLASS (valent_clipboard_parent_class)->dispose (object);
}

static void
valent_clipboard_finalize (GObject *object)
{
  ValentClipboard *self = VALENT_CLIPBOARD (object);

  g_clear_object (&self->cancellable);
  g_clear_object (&self->default_adapter);

  G_OBJECT_CLASS (valent_clipboard_parent_class)->finalize (object);
}

static void
valent_clipboard_class_init (ValentClipboardClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentComponentClass *component_class = VALENT_COMPONENT_CLASS (klass);

  object_class->dispose = valent_clipboard_dispose;
  object_class->finalize = valent_clipboard_finalize;

  component_class->extension_added = valent_clipboard_extension_added;
  component_class->extension_removed = valent_clipboard_extension_removed;

  /**
   * ValentClipboard::changed:
   * @clipboard: a #ValentClipboard
   *
   * #ValentClipboard::changed is emitted when the content of the default
   * #ValentClipboardAdapter changes.
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
  self->cancellable = g_cancellable_new ();
}


/**
 * valent_clipboard_get_text_async:
 * @clipboard: a #ValentClipboard
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): a #GAsyncReadyCallback
 * @user_data: (closure): user supplied data
 *
 * Get the text content of @clipboard.
 */
void
valent_clipboard_get_text_async (ValentClipboard     *clipboard,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
  g_autoptr (GTask) task = NULL;

  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_CLIPBOARD (clipboard));

  if G_UNLIKELY (clipboard->default_adapter == NULL)
    {
      g_task_report_new_error (clipboard, callback, user_data,
                               valent_clipboard_get_text_async,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_SUPPORTED,
                               "No clipboard adapter");
      return;
    }

  task = g_task_new (clipboard, cancellable, callback, user_data);
  valent_clipboard_adapter_get_text_async (clipboard->default_adapter,
                                           cancellable,
                                           (GAsyncReadyCallback)get_text_cb,
                                           g_steal_pointer (&task));

  VALENT_EXIT;
}

/**
 * valent_clipboard_get_text_finish:
 * @clipboard: a #ValentClipboard
 * @result: a #GAsyncResult
 * @error: (nullable): a #GError
 *
 * Finish an operation started with valent_clipboard_get_text_async().
 *
 * Returns: (transfer full) (nullable): the text content
 */
char *
valent_clipboard_get_text_finish (ValentClipboard  *clipboard,
                                  GAsyncResult     *result,
                                  GError          **error)
{
  char *ret;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_CLIPBOARD (clipboard), NULL);
  g_return_val_if_fail (g_task_is_valid (result, clipboard), NULL);

  ret = g_task_propagate_pointer (G_TASK (result), error);

  VALENT_RETURN (ret);
}

/**
 * valent_clipboard_set_text:
 * @clipboard: a #ValentClipboard
 * @text: (nullable): text content
 *
 * Set the content of the clipboard to @text.
 */
void
valent_clipboard_set_text (ValentClipboard *clipboard,
                           const char      *text)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_CLIPBOARD (clipboard));

  if G_UNLIKELY (clipboard->default_adapter == NULL)
    {
      g_warning ("No clipboard adapter");
      VALENT_EXIT;
    }

  valent_clipboard_adapter_set_text (clipboard->default_adapter, text);

  VALENT_EXIT;
}

/**
 * valent_clipboard_get_timestamp:
 * @clipboard: a #ValentClipboard
 *
 * Get the timestamp of the current clipboard content, in milliseconds since the
 * UNIX epoch.
 *
 * Returns: a UNIX epoch timestamp (ms)
 */
gint64
valent_clipboard_get_timestamp (ValentClipboard *clipboard)
{
  gint64 ret = 0;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_CLIPBOARD (clipboard), 0);

  if G_UNLIKELY (clipboard->default_adapter == NULL)
    VALENT_RETURN (ret);

  ret = valent_clipboard_adapter_get_timestamp (clipboard->default_adapter);

  VALENT_RETURN (ret);
}

/**
 * valent_clipboard_get_default:
 *
 * Get the default #ValentClipboard.
 *
 * Returns: (transfer none): The default clipboard
 */
ValentClipboard *
valent_clipboard_get_default (void)
{
  if (default_clipboard == NULL)
    {
      default_clipboard = g_object_new (VALENT_TYPE_CLIPBOARD,
                                        "plugin-context", "clipboard",
                                        "plugin-type",    VALENT_TYPE_CLIPBOARD_ADAPTER,
                                        NULL);

      g_object_add_weak_pointer (G_OBJECT (default_clipboard),
                                 (gpointer)&default_clipboard);
    }

  return default_clipboard;
}

