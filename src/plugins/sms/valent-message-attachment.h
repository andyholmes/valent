// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <gio/gio.h>

G_BEGIN_DECLS

#define VALENT_TYPE_MESSAGE_ATTACHMENT (valent_message_attachment_get_type())

G_DECLARE_FINAL_TYPE (ValentMessageAttachment, valent_message_attachment, VALENT, MESSAGE_ATTACHMENT, GObject)

ValentMessageAttachment * valent_message_attachment_new         (const char              *iri);
const char              * valent_message_attachment_get_iri     (ValentMessageAttachment *attachment);
GFile                   * valent_message_attachment_get_file    (ValentMessageAttachment *attachment);
void                      valent_message_attachment_set_file    (ValentMessageAttachment *attachment,
                                                                 GFile                   *file);
GBytes                  * valent_message_attachment_get_preview (ValentMessageAttachment *attachment);
void                      valent_message_attachment_set_preview (ValentMessageAttachment *attachment,
                                                                 GBytes                  *preview);

G_END_DECLS
