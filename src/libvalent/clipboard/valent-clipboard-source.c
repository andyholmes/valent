// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-clipboard-source"

#include "config.h"

#include <gio/gio.h>
#include <libpeas/peas.h>
#include <libvalent-core.h>

#include "valent-clipboard-source.h"


/**
 * SECTION:valentclipboard-source
 * @short_description: Interface for clipboard sources
 * @title: ValentClipboardSource
 * @stability: Unstable
 * @include: libvalent-clipboard.h
 *
 * The #ValentClipboardSource interface should be implemented by libpeas plugins
 * that operate at the desktop level. This generally means providing access to
 * the desktop session clipboard.
 *
 * ## .plugin File ##
 *
 * Clipboard source require no special entries in the `.plugin` file.
 */

typedef struct
{
  PeasPluginInfo *plugin_info;
} ValentClipboardSourcePrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (ValentClipboardSource, valent_clipboard_source, G_TYPE_OBJECT)

/**
 * ValentClipboardSourceClass:
 * @changed: class closure for #ValentClipboardSource::changed signal
 * @get_text_async: the virtual function pointer for valent_clipboard_source_get_text_async()
 * @get_text_finish: the virtual function pointer for valent_clipboard_source_get_text_finish()
 * @set_text: the virtual function pointer for valent_clipboard_source_set_text()
 *
 * The virtual function table for #ValentClipboardSource.
 */

enum {
  PROP_0,
  PROP_PLUGIN_INFO,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

enum {
  CHANGED,
  N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0, };


/* LCOV_EXCL_START */
static void
valent_clipboard_source_real_get_text_async (ValentClipboardSource *source,
                                             GCancellable          *cancellable,
                                             GAsyncReadyCallback    callback,
                                             gpointer               user_data)
{
  g_task_report_new_error (source, callback, user_data,
                           valent_clipboard_source_real_get_text_async,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "%s does not implement get_text_async",
                           G_OBJECT_TYPE_NAME (source));
}

static char *
valent_clipboard_source_real_get_text_finish (ValentClipboardSource  *source,
                                              GAsyncResult           *result,
                                              GError                **error)
{
  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
valent_clipboard_source_real_set_text (ValentClipboardSource *source,
                                       const char            *text)
{
}
/* LCOV_EXCL_STOP */

/*
 * GObject
 */
static void
valent_clipboard_source_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  ValentClipboardSource *self = VALENT_CLIPBOARD_SOURCE (object);
  ValentClipboardSourcePrivate *priv = valent_clipboard_source_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_PLUGIN_INFO:
      g_value_set_boxed (value, priv->plugin_info);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_clipboard_source_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  ValentClipboardSource *self = VALENT_CLIPBOARD_SOURCE (object);
  ValentClipboardSourcePrivate *priv = valent_clipboard_source_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_PLUGIN_INFO:
      priv->plugin_info = g_value_get_boxed (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_clipboard_source_class_init (ValentClipboardSourceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = valent_clipboard_source_get_property;
  object_class->set_property = valent_clipboard_source_set_property;

  klass->get_text_async = valent_clipboard_source_real_get_text_async;
  klass->get_text_finish = valent_clipboard_source_real_get_text_finish;
  klass->set_text = valent_clipboard_source_real_set_text;

  /**
   * ValentClipboardSource:plugin-info:
   *
   * The #PeasPluginInfo describing this source.
   */
  properties [PROP_PLUGIN_INFO] =
    g_param_spec_boxed ("plugin-info",
                        "Plugin Info",
                        "Plugin Info",
                        PEAS_TYPE_PLUGIN_INFO,
                        (G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);

  /**
   * ValentClipboardSource::changed:
   * @source: a #ValentClipboardSource
   *
   * #ValentClipboardSource::changed is emitted when @source changes.
   */
  signals [CHANGED] =
    g_signal_new ("changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ValentClipboardSourceClass, changed),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);
}

static void
valent_clipboard_source_init (ValentClipboardSource *source)
{
}

/**
 * valent_clipboard_source_emit_changed:
 * @source: a #ValentClipboardSource
 *
 * Emits the #ValentClipboardSource::changed signal on @source.
 */
void
valent_clipboard_source_emit_changed (ValentClipboardSource *source)
{
  g_return_if_fail (VALENT_IS_CLIPBOARD_SOURCE (source));

  g_signal_emit (G_OBJECT (source), signals [CHANGED], 0);
}

/**
 * valent_clipboard_source_get_text_async:
 * @source: a #ValentClipboardSource
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): a #GAsyncReadyCallback
 * @user_data: (closure): user supplied data
 *
 * Get the text content of @source.
 */
void
valent_clipboard_source_get_text_async (ValentClipboardSource *source,
                                        GCancellable          *cancellable,
                                        GAsyncReadyCallback    callback,
                                        gpointer               user_data)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_CLIPBOARD_SOURCE (source));

  VALENT_CLIPBOARD_SOURCE_GET_CLASS (source)->get_text_async (source,
                                                              cancellable,
                                                              callback,
                                                              user_data);
  VALENT_EXIT;
}

/**
 * valent_clipboard_source_get_text_finish:
 * @source: a #ValentClipboardSource
 * @result: a #GAsyncResult
 * @error: (nullable): a #GError
 *
 * Get the text content of @source.
 *
 * Returns: (transfer full) (nullable): the text content
 */
char *
valent_clipboard_source_get_text_finish (ValentClipboardSource  *source,
                                         GAsyncResult           *result,
                                         GError                **error)
{
  char *ret;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_CLIPBOARD_SOURCE (source), NULL);
  g_return_val_if_fail (g_task_is_valid (result, source), NULL);
  g_return_val_if_fail (*error == NULL || error == NULL, NULL);

  ret = VALENT_CLIPBOARD_SOURCE_GET_CLASS (source)->get_text_finish (source, result, error);

  VALENT_RETURN (ret);
}

/**
 * valent_clipboard_source_set_text:
 * @source: a #ValentClipboardSource
 * @text: (nullable): text content
 *
 * Set the text content of @source to @text. The default handler simply caches
 * @text and emits #GObject::notify.
 */
void
valent_clipboard_source_set_text (ValentClipboardSource *source,
                                  const char            *text)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_CLIPBOARD_SOURCE (source));

  VALENT_CLIPBOARD_SOURCE_GET_CLASS (source)->set_text (source, text);

  VALENT_EXIT;
}

