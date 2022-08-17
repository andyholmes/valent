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
  GStrv          (*get_mimetypes)      (ValentClipboardAdapter  *adapter);
  gint64         (*get_timestamp)      (ValentClipboardAdapter  *adapter);
  void           (*read_bytes)         (ValentClipboardAdapter  *adapter,
                                        const char              *mimetype,
                                        GCancellable            *cancellable,
                                        GAsyncReadyCallback      callback,
                                        gpointer                 user_data);
  GBytes       * (*read_bytes_finish)  (ValentClipboardAdapter  *adapter,
                                        GAsyncResult            *result,
                                        GError                 **error);
  void           (*write_bytes)        (ValentClipboardAdapter  *adapter,
                                        const char              *mimetype,
                                        GBytes                  *bytes,
                                        GCancellable            *cancellable,
                                        GAsyncReadyCallback      callback,
                                        gpointer                 user_data);
  gboolean       (*write_bytes_finish) (ValentClipboardAdapter  *adapter,
                                        GAsyncResult            *result,
                                        GError                 **error);
  void           (*read_text)          (ValentClipboardAdapter  *adapter,
                                        GCancellable            *cancellable,
                                        GAsyncReadyCallback      callback,
                                        gpointer                 user_data);
  char         * (*read_text_finish)   (ValentClipboardAdapter  *adapter,
                                        GAsyncResult            *result,
                                        GError                 **error);
  void           (*write_text)         (ValentClipboardAdapter  *adapter,
                                        const char              *text,
                                        GCancellable            *cancellable,
                                        GAsyncReadyCallback      callback,
                                        gpointer                 user_data);
  gboolean       (*write_text_finish)  (ValentClipboardAdapter  *adapter,
                                        GAsyncResult            *result,
                                        GError                 **error);

  /* signals */
  void           (*changed)            (ValentClipboardAdapter  *adapter);
};

VALENT_AVAILABLE_IN_1_0
void       valent_clipboard_adapter_emit_changed       (ValentClipboardAdapter  *adapter);
VALENT_AVAILABLE_IN_1_0
GStrv      valent_clipboard_adapter_get_mimetypes      (ValentClipboardAdapter  *adapter);
VALENT_AVAILABLE_IN_1_0
gint64     valent_clipboard_adapter_get_timestamp      (ValentClipboardAdapter  *adapter);
VALENT_AVAILABLE_IN_1_0
void       valent_clipboard_adapter_read_bytes         (ValentClipboardAdapter  *adapter,
                                                        const char              *mimetype,
                                                        GCancellable            *cancellable,
                                                        GAsyncReadyCallback      callback,
                                                        gpointer                 user_data);
VALENT_AVAILABLE_IN_1_0
GBytes   * valent_clipboard_adapter_read_bytes_finish  (ValentClipboardAdapter  *adapter,
                                                        GAsyncResult            *result,
                                                        GError                 **error);
VALENT_AVAILABLE_IN_1_0
void       valent_clipboard_adapter_write_bytes        (ValentClipboardAdapter  *adapter,
                                                        const char              *mimetype,
                                                        GBytes                  *bytes,
                                                        GCancellable            *cancellable,
                                                        GAsyncReadyCallback      callback,
                                                        gpointer                 user_data);
VALENT_AVAILABLE_IN_1_0
gboolean   valent_clipboard_adapter_write_bytes_finish (ValentClipboardAdapter  *adapter,
                                                        GAsyncResult            *result,
                                                        GError                 **error);
VALENT_AVAILABLE_IN_1_0
void       valent_clipboard_adapter_read_text          (ValentClipboardAdapter  *adapter,
                                                        GCancellable            *cancellable,
                                                        GAsyncReadyCallback      callback,
                                                        gpointer                 user_data);
VALENT_AVAILABLE_IN_1_0
char     * valent_clipboard_adapter_read_text_finish   (ValentClipboardAdapter  *adapter,
                                                        GAsyncResult            *result,
                                                        GError                 **error);
VALENT_AVAILABLE_IN_1_0
void      valent_clipboard_adapter_write_text          (ValentClipboardAdapter  *adapter,
                                                        const char              *text,
                                                        GCancellable            *cancellable,
                                                        GAsyncReadyCallback      callback,
                                                        gpointer                 user_data);
VALENT_AVAILABLE_IN_1_0
gboolean   valent_clipboard_adapter_write_text_finish  (ValentClipboardAdapter  *adapter,
                                                        GAsyncResult            *result,
                                                        GError                 **error);

G_END_DECLS

