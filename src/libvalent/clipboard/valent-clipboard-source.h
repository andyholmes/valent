// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_CLIPBOARD_INSIDE) && !defined (VALENT_CLIPBOARD_COMPILATION)
# error "Only <libvalent-clipboard.h> can be included directly."
#endif

#include <libvalent-core.h>

#include <gio/gio.h>

G_BEGIN_DECLS

#define VALENT_TYPE_CLIPBOARD_SOURCE (valent_clipboard_source_get_type())

G_DECLARE_DERIVABLE_TYPE (ValentClipboardSource, valent_clipboard_source, VALENT, CLIPBOARD_SOURCE, GObject)

struct _ValentClipboardSourceClass
{
  GObjectClass   parent_class;

  /* virtual functions */
  void           (*get_text_async)  (ValentClipboardSource  *source,
                                     GCancellable           *cancellable,
                                     GAsyncReadyCallback     callback,
                                     gpointer                user_data);
  char         * (*get_text_finish) (ValentClipboardSource  *source,
                                     GAsyncResult           *result,
                                     GError                **error);
  void           (*set_text)        (ValentClipboardSource  *source,
                                     const char             *text);

  /* signals */
  void           (*changed)         (ValentClipboardSource  *source);
};

void   valent_clipboard_source_emit_changed    (ValentClipboardSource  *source);
void   valent_clipboard_source_get_text_async  (ValentClipboardSource  *source,
                                                GCancellable           *cancellable,
                                                GAsyncReadyCallback     callback,
                                                gpointer                user_data);
char * valent_clipboard_source_get_text_finish (ValentClipboardSource  *source,
                                                GAsyncResult           *result,
                                                GError                **error);
void   valent_clipboard_source_set_text        (ValentClipboardSource  *source,
                                                const char             *text);

G_END_DECLS

