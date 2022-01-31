// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_CLIPBOARD_INSIDE) && !defined (VALENT_CLIPBOARD_COMPILATION)
# error "Only <libvalent-clipboard.h> can be included directly."
#endif

#include <libvalent-core.h>

G_BEGIN_DECLS

#define VALENT_TYPE_CLIPBOARD_ADAPTER (valent_clipboard_adapter_get_type())

VALENT_AVAILABLE_IN_1_0
G_DECLARE_DERIVABLE_TYPE (ValentClipboardAdapter, valent_clipboard_adapter, VALENT, CLIPBOARD_ADAPTER, GObject)

struct _ValentClipboardAdapterClass
{
  GObjectClass   parent_class;

  /* virtual functions */
  void           (*get_text_async)  (ValentClipboardAdapter  *adapter,
                                     GCancellable            *cancellable,
                                     GAsyncReadyCallback      callback,
                                     gpointer                 user_data);
  char         * (*get_text_finish) (ValentClipboardAdapter  *adapter,
                                     GAsyncResult            *result,
                                     GError                 **error);
  void           (*set_text)        (ValentClipboardAdapter  *adapter,
                                     const char              *text);
  gint64         (*get_timestamp)   (ValentClipboardAdapter  *adapter);

  /* signals */
  void           (*changed)         (ValentClipboardAdapter  *adapter);
};

VALENT_AVAILABLE_IN_1_0
void     valent_clipboard_adapter_emit_changed    (ValentClipboardAdapter  *adapter);
VALENT_AVAILABLE_IN_1_0
void     valent_clipboard_adapter_get_text_async  (ValentClipboardAdapter  *adapter,
                                                   GCancellable            *cancellable,
                                                   GAsyncReadyCallback      callback,
                                                   gpointer                 user_data);
VALENT_AVAILABLE_IN_1_0
char   * valent_clipboard_adapter_get_text_finish (ValentClipboardAdapter  *adapter,
                                                   GAsyncResult            *result,
                                                   GError                 **error);
VALENT_AVAILABLE_IN_1_0
void     valent_clipboard_adapter_set_text        (ValentClipboardAdapter  *adapter,
                                                   const char              *text);
VALENT_AVAILABLE_IN_1_0
gint64   valent_clipboard_adapter_get_timestamp   (ValentClipboardAdapter  *adapter);

G_END_DECLS

