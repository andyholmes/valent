// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <gio/gio.h>
#include <libvalent-core.h>

#include "valent-sms-message.h"

G_BEGIN_DECLS

#define VALENT_TYPE_SMS_STORE (valent_sms_store_get_type())

G_DECLARE_FINAL_TYPE (ValentSmsStore, valent_sms_store, VALENT, SMS_STORE, ValentData)

ValentSmsStore   * valent_sms_store_new             (ValentData           *prefix);

void               valent_sms_store_add_message     (ValentSmsStore       *store,
                                                     ValentSmsMessage     *message);
void               valent_sms_store_remove_message  (ValentSmsStore       *store,
                                                     gint64                thread_id,
                                                     gint64                message_id);
void               valent_sms_store_remove_messages (ValentSmsStore       *store,
                                                     gint64                thread_id);
GPtrArray        * valent_sms_store_find            (ValentSmsStore       *store,
                                                     const char           *query,
                                                     GError              **error);
void               valent_sms_store_find_async      (ValentSmsStore       *store,
                                                     const char           *query,
                                                     GCancellable         *cancellable,
                                                     GAsyncReadyCallback   callback,
                                                     gpointer              user_data);
GPtrArray        * valent_sms_store_find_finish     (ValentSmsStore       *store,
                                                     GAsyncResult         *result,
                                                     GError              **error);
ValentSmsMessage * valent_sms_store_get_message     (ValentSmsStore       *store,
                                                     gint64                message_id);
GListModel       * valent_sms_store_get_summary     (ValentSmsStore       *store);
GQueue           * valent_sms_store_dup_thread      (ValentSmsStore       *store,
                                                     gint64                thread_id);
GQueue           * valent_sms_store_get_thread      (ValentSmsStore       *store,
                                                     gint64                thread_id);
gint64             valent_sms_store_get_thread_date (ValentSmsStore       *store,
                                                     gint64                thread_id);

G_END_DECLS
