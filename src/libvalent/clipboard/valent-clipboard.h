// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_CLIPBOARD_INSIDE) && !defined (VALENT_CLIPBOARD_COMPILATION)
# error "Only <libvalent-clipboard.h> can be included directly."
#endif

#include <libvalent-core.h>

G_BEGIN_DECLS

#define VALENT_TYPE_CLIPBOARD (valent_clipboard_get_type ())

VALENT_AVAILABLE_IN_1_0
G_DECLARE_FINAL_TYPE (ValentClipboard, valent_clipboard, VALENT, CLIPBOARD, ValentComponent)

VALENT_AVAILABLE_IN_1_0
ValentClipboard * valent_clipboard_get_default     (void);

VALENT_AVAILABLE_IN_1_0
void              valent_clipboard_get_text_async  (ValentClipboard      *clipboard,
                                                    GCancellable         *cancellable,
                                                    GAsyncReadyCallback   callback,
                                                    gpointer              user_data);
VALENT_AVAILABLE_IN_1_0
char            * valent_clipboard_get_text_finish (ValentClipboard      *clipboard,
                                                    GAsyncResult         *result,
                                                    GError              **error);
VALENT_AVAILABLE_IN_1_0
void              valent_clipboard_set_text        (ValentClipboard      *clipboard,
                                                    const char           *text);
VALENT_AVAILABLE_IN_1_0
gint64            valent_clipboard_get_timestamp   (ValentClipboard      *clipboard);

G_END_DECLS

