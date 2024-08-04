// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_INSIDE) && !defined (VALENT_COMPILATION)
# error "Only <valent.h> can be included directly."
#endif

#include <gio/gio.h>

#include "../core/valent-object.h"

G_BEGIN_DECLS

#define VALENT_TYPE_MESSAGE_ATTACHMENT (valent_message_attachment_get_type())

VALENT_AVAILABLE_IN_1_0
G_DECLARE_FINAL_TYPE (ValentMessageAttachment, valent_message_attachment, VALENT, MESSAGE_ATTACHMENT, ValentObject)

VALENT_AVAILABLE_IN_1_0
GFile                   * valent_message_attachment_get_file    (ValentMessageAttachment *attachment);
VALENT_AVAILABLE_IN_1_0
void                      valent_message_attachment_set_file    (ValentMessageAttachment *attachment,
                                                                 GFile                   *file);
VALENT_AVAILABLE_IN_1_0
GIcon                   * valent_message_attachment_get_preview (ValentMessageAttachment *attachment);
VALENT_AVAILABLE_IN_1_0
void                      valent_message_attachment_set_preview (ValentMessageAttachment *attachment,
                                                                 GIcon                   *preview);

G_END_DECLS
