// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-clipboard-adapter"

#include "config.h"

#include <gio/gio.h>
#include <libpeas/peas.h>
#include <libvalent-core.h>

#include "valent-clipboard-adapter.h"


/**
 * SECTION:valentclipboardadapter
 * @short_description: Interface for clipboard adapters
 * @title: ValentClipboardAdapter
 * @stability: Unstable
 * @include: libvalent-clipboard.h
 *
 * #ValentClipboardAdapter is a base class for plugins that provide read-write
 * access to the desktop clipboard for use by remote devices.
 *
 * ## .plugin File ##
 *
 * Plugins require no special entries in the `.plugin` file, but may specify the
 * `X-ClipboardAdapterPriority` field with an integer value. The implementation
 * with the lowest value will take precedence.
 */

typedef struct
{
  PeasPluginInfo *plugin_info;
} ValentClipboardAdapterPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (ValentClipboardAdapter, valent_clipboard_adapter, G_TYPE_OBJECT)

/**
 * ValentClipboardAdapterClass:
 * @changed: class closure for #ValentClipboardAdapter::changed signal
 * @get_text_async: the virtual function pointer for valent_clipboard_adapter_get_text_async()
 * @get_text_finish: the virtual function pointer for valent_clipboard_adapter_get_text_finish()
 * @set_text: the virtual function pointer for valent_clipboard_adapter_set_text()
 * @get_timestamp: the virtual function pointer for valent_clipboard_adapter_get_timestamp()
 *
 * The virtual function table for #ValentClipboardAdapter.
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
valent_clipboard_adapter_real_get_text_async (ValentClipboardAdapter *adapter,
                                              GCancellable           *cancellable,
                                              GAsyncReadyCallback     callback,
                                              gpointer                user_data)
{
  g_task_report_new_error (adapter, callback, user_data,
                           valent_clipboard_adapter_real_get_text_async,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "%s does not implement get_text_async",
                           G_OBJECT_TYPE_NAME (adapter));
}

static char *
valent_clipboard_adapter_real_get_text_finish (ValentClipboardAdapter  *adapter,
                                               GAsyncResult            *result,
                                               GError                 **error)
{
  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
valent_clipboard_adapter_real_set_text (ValentClipboardAdapter *adapter,
                                        const char             *text)
{
  g_warning ("%s does not implement set_text", G_OBJECT_TYPE_NAME (adapter));
}

static gint64
valent_clipboard_adapter_real_get_timestamp (ValentClipboardAdapter *adapter)
{
  return 0;
}
/* LCOV_EXCL_STOP */

/*
 * GObject
 */
static void
valent_clipboard_adapter_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  ValentClipboardAdapter *self = VALENT_CLIPBOARD_ADAPTER (object);
  ValentClipboardAdapterPrivate *priv = valent_clipboard_adapter_get_instance_private (self);

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
valent_clipboard_adapter_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  ValentClipboardAdapter *self = VALENT_CLIPBOARD_ADAPTER (object);
  ValentClipboardAdapterPrivate *priv = valent_clipboard_adapter_get_instance_private (self);

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
valent_clipboard_adapter_class_init (ValentClipboardAdapterClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = valent_clipboard_adapter_get_property;
  object_class->set_property = valent_clipboard_adapter_set_property;

  klass->get_text_async = valent_clipboard_adapter_real_get_text_async;
  klass->get_text_finish = valent_clipboard_adapter_real_get_text_finish;
  klass->set_text = valent_clipboard_adapter_real_set_text;
  klass->get_timestamp = valent_clipboard_adapter_real_get_timestamp;

  /**
   * ValentClipboardAdapter:plugin-info:
   *
   * The #PeasPluginInfo describing this adapter.
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
   * ValentClipboardAdapter::changed:
   * @adapter: a #ValentClipboardAdapter
   *
   * #ValentClipboardAdapter::changed is emitted when @adapter changes.
   */
  signals [CHANGED] =
    g_signal_new ("changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ValentClipboardAdapterClass, changed),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);
}

static void
valent_clipboard_adapter_init (ValentClipboardAdapter *adapter)
{
}

/**
 * valent_clipboard_adapter_emit_changed:
 * @adapter: a #ValentClipboardAdapter
 *
 * Emits the #ValentClipboardAdapter::changed signal on @adapter.
 */
void
valent_clipboard_adapter_emit_changed (ValentClipboardAdapter *adapter)
{
  g_return_if_fail (VALENT_IS_CLIPBOARD_ADAPTER (adapter));

  g_signal_emit (G_OBJECT (adapter), signals [CHANGED], 0);
}

/**
 * valent_clipboard_adapter_get_text_async:
 * @adapter: a #ValentClipboardAdapter
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): a #GAsyncReadyCallback
 * @user_data: (closure): user supplied data
 *
 * Get the text content of @adapter.
 */
void
valent_clipboard_adapter_get_text_async (ValentClipboardAdapter *adapter,
                                         GCancellable           *cancellable,
                                         GAsyncReadyCallback     callback,
                                         gpointer                user_data)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_CLIPBOARD_ADAPTER (adapter));

  VALENT_CLIPBOARD_ADAPTER_GET_CLASS (adapter)->get_text_async (adapter,
                                                                cancellable,
                                                                callback,
                                                                user_data);
  VALENT_EXIT;
}

/**
 * valent_clipboard_adapter_get_text_finish:
 * @adapter: a #ValentClipboardAdapter
 * @result: a #GAsyncResult
 * @error: (nullable): a #GError
 *
 * Get the text content of @adapter.
 *
 * Returns: (transfer full) (nullable): the text content
 */
char *
valent_clipboard_adapter_get_text_finish (ValentClipboardAdapter  *adapter,
                                          GAsyncResult            *result,
                                          GError                 **error)
{
  char *ret;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_CLIPBOARD_ADAPTER (adapter), NULL);
  g_return_val_if_fail (g_task_is_valid (result, adapter), NULL);
  g_return_val_if_fail (*error == NULL || error == NULL, NULL);

  ret = VALENT_CLIPBOARD_ADAPTER_GET_CLASS (adapter)->get_text_finish (adapter, result, error);

  VALENT_RETURN (ret);
}

/**
 * valent_clipboard_adapter_set_text:
 * @adapter: a #ValentClipboardAdapter
 * @text: (nullable): text content
 *
 * Set the text content of @adapter to @text. The default handler simply caches
 * @text and emits #GObject::notify.
 */
void
valent_clipboard_adapter_set_text (ValentClipboardAdapter *adapter,
                                   const char             *text)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_CLIPBOARD_ADAPTER (adapter));

  VALENT_CLIPBOARD_ADAPTER_GET_CLASS (adapter)->set_text (adapter, text);

  VALENT_EXIT;
}

/**
 * valent_clipboard_adapter_get_timestamp:
 * @adapter: a #ValentClipboardAdapter
 *
 * Get the timestamp of the current clipboard content, in milliseconds since the
 * UNIX epoch.
 *
 * Returns: a UNIX epoch timestamp (ms)
 */
gint64
valent_clipboard_adapter_get_timestamp (ValentClipboardAdapter *adapter)
{
  gint64 ret;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_CLIPBOARD_ADAPTER (adapter), 0);

  ret = VALENT_CLIPBOARD_ADAPTER_GET_CLASS (adapter)->get_timestamp (adapter);

  VALENT_RETURN (ret);
}

