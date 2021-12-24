// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_CLIPBOARD_INSIDE) && !defined (VALENT_CLIPBOARD_COMPILATION)
# error "Only <libvalent-clipboard.h> can be included directly."
#endif

#include <libvalent-core.h>

G_BEGIN_DECLS

#define VALENT_TYPE_CLIPBOARD (valent_clipboard_get_type ())

G_DECLARE_FINAL_TYPE (ValentClipboard, valent_clipboard, VALENT, CLIPBOARD, ValentComponent)

void              valent_clipboard_get_text_async  (ValentClipboard      *clipboard,
                                                    GCancellable         *cancellable,
                                                    GAsyncReadyCallback   callback,
                                                    gpointer              user_data);
char            * valent_clipboard_get_text_finish (ValentClipboard      *clipboard,
                                                    GAsyncResult         *result,
                                                    GError              **error);
void              valent_clipboard_set_text        (ValentClipboard      *clipboard,
                                                    const char           *text);
gint64            valent_clipboard_get_timestamp   (ValentClipboard      *clipboard);

ValentClipboard * valent_clipboard_get_default     (void);

G_END_DECLS

