// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <gio/gio.h>

#include "valent-message.h"
#include "valent-sms-store.h"

G_BEGIN_DECLS

#define VALENT_TYPE_MESSAGE_THREAD (valent_message_thread_get_type())

G_DECLARE_FINAL_TYPE (ValentMessageThread, valent_message_thread, VALENT, MESSAGE_THREAD, GObject)

GListModel     * valent_message_thread_new       (ValentSmsStore      *store,
                                                  gint64               id);

ValentSmsStore * valent_message_thread_get_store (ValentMessageThread *thread);
gint64           valent_message_thread_get_id    (ValentMessageThread *thread);
void             valent_message_thread_set_id    (ValentMessageThread *thread,
                                                  gint64               id);

G_END_DECLS
