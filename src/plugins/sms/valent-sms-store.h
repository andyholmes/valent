// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <gio/gio.h>
#include <libvalent-core.h>

#include "valent-message.h"

G_BEGIN_DECLS

#define VALENT_TYPE_SMS_STORE (valent_sms_store_get_type())

G_DECLARE_FINAL_TYPE (ValentSmsStore, valent_sms_store, VALENT, SMS_STORE, ValentData)

ValentSmsStore * valent_sms_store_new                   (ValentData           *prefix);

void             valent_sms_store_add_message           (ValentSmsStore       *store,
                                                         ValentMessage        *message,
                                                         GCancellable         *cancellable,
                                                         GAsyncReadyCallback   callback,
                                                         gpointer              user_data);
void             valent_sms_store_add_messages          (ValentSmsStore       *store,
                                                         GPtrArray            *messages,
                                                         GCancellable         *cancellable,
                                                         GAsyncReadyCallback   callback,
                                                         gpointer              user_data);
gboolean         valent_sms_store_add_messages_finish   (ValentSmsStore       *store,
                                                         GAsyncResult         *result,
                                                         GError              **error);
void             valent_sms_store_remove_message        (ValentSmsStore       *store,
                                                         gint64                message_id,
                                                         GCancellable         *cancellable,
                                                         GAsyncReadyCallback   callback,
                                                         gpointer              user_data);
gboolean         valent_sms_store_remove_message_finish (ValentSmsStore       *store,
                                                         GAsyncResult         *result,
                                                         GError              **error);
void             valent_sms_store_remove_thread         (ValentSmsStore       *store,
                                                         gint64                thread_id,
                                                         GCancellable         *cancellable,
                                                         GAsyncReadyCallback   callback,
                                                         gpointer              user_data);
gboolean         valent_sms_store_remove_thread_finish  (ValentSmsStore       *store,
                                                         GAsyncResult         *result,
                                                         GError              **error);
void             valent_sms_store_find_messages         (ValentSmsStore       *store,
                                                         const char           *query,
                                                         GCancellable         *cancellable,
                                                         GAsyncReadyCallback   callback,
                                                         gpointer              user_data);
GPtrArray      * valent_sms_store_find_messages_finish  (ValentSmsStore       *store,
                                                         GAsyncResult         *result,
                                                         GError              **error);
void             valent_sms_store_get_message           (ValentSmsStore       *store,
                                                         gint64                message_id,
                                                         GCancellable         *cancellable,
                                                         GAsyncReadyCallback   callback,
                                                         gpointer              user_data);
ValentMessage  * valent_sms_store_get_message_finish    (ValentSmsStore       *store,
                                                         GAsyncResult         *result,
                                                         GError              **error);
GListModel     * valent_sms_store_get_summary           (ValentSmsStore       *store);
GListModel     * valent_sms_store_get_thread            (ValentSmsStore       *store,
                                                         gint64                thread_id);
gint64           valent_sms_store_get_thread_date       (ValentSmsStore       *store,
                                                         gint64                thread_id);
void             valent_sms_store_emit_message_added    (ValentSmsStore       *store,
                                                         ValentMessage        *message);
void             valent_sms_store_emit_message_removed  (ValentSmsStore       *store,
                                                         ValentMessage        *message);
void             valent_sms_store_emit_message_changed  (ValentSmsStore       *store,
                                                         ValentMessage        *message);

G_END_DECLS
