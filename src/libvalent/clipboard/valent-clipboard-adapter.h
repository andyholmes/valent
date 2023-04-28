// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_INSIDE) && !defined (VALENT_COMPILATION)
# error "Only <valent.h> can be included directly."
#endif

#include "../core/valent-extension.h"

G_BEGIN_DECLS

#define VALENT_TYPE_CLIPBOARD_ADAPTER (valent_clipboard_adapter_get_type())

VALENT_AVAILABLE_IN_1_0
G_DECLARE_DERIVABLE_TYPE (ValentClipboardAdapter, valent_clipboard_adapter, VALENT, CLIPBOARD_ADAPTER, ValentExtension)

struct _ValentClipboardAdapterClass
{
  ValentExtensionClass   parent_class;

  /* virtual functions */
  GStrv                  (*get_mimetypes)      (ValentClipboardAdapter  *adapter);
  int64_t                (*get_timestamp)      (ValentClipboardAdapter  *adapter);
  void                   (*read_bytes)         (ValentClipboardAdapter  *adapter,
                                                const char              *mimetype,
                                                GCancellable            *cancellable,
                                                GAsyncReadyCallback      callback,
                                                gpointer                 user_data);
  GBytes               * (*read_bytes_finish)  (ValentClipboardAdapter  *adapter,
                                                GAsyncResult            *result,
                                                GError                 **error);
  void                   (*write_bytes)        (ValentClipboardAdapter  *adapter,
                                                const char              *mimetype,
                                                GBytes                  *bytes,
                                                GCancellable            *cancellable,
                                                GAsyncReadyCallback      callback,
                                                gpointer                 user_data);
  gboolean               (*write_bytes_finish) (ValentClipboardAdapter  *adapter,
                                                GAsyncResult            *result,
                                                GError                 **error);

  /* signals */
  void                   (*changed)            (ValentClipboardAdapter  *adapter);

  /*< private >*/
  gpointer               padding[8];
};

VALENT_AVAILABLE_IN_1_0
void       valent_clipboard_adapter_changed            (ValentClipboardAdapter  *adapter);
VALENT_AVAILABLE_IN_1_0
GStrv      valent_clipboard_adapter_get_mimetypes      (ValentClipboardAdapter  *adapter);
VALENT_AVAILABLE_IN_1_0
int64_t    valent_clipboard_adapter_get_timestamp      (ValentClipboardAdapter  *adapter);
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

G_END_DECLS

