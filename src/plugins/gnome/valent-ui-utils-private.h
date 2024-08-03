// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <adwaita.h>
#include <valent.h>

G_BEGIN_DECLS

/**
 * ValentTimeFlag:
 * @TOTEM_TIME_FLAG_NONE: Default behaviour
 * @TOTEM_TIME_FLAG_REMAINING: Time remaining
 * @TOTEM_TIME_FLAG_FORCE_HOUR: Always include the hourly duration
 * @TOTEM_TIME_FLAG_MSECS: Always include the millisecond duration
 *
 * Time duration flags.
 *
 * Since: 1.0
 */
typedef enum {
  TOTEM_TIME_FLAG_NONE,
  TOTEM_TIME_FLAG_REMAINING  = (1 << 0),
  TOTEM_TIME_FLAG_FORCE_HOUR = (1 << 2),
  TOTEM_TIME_FLAG_MSECS      = (1 << 3),
} TotemTimeFlag;

gboolean     valent_ui_init                               (void);
char       * valent_string_to_markup                      (const char *text);

char       * valent_media_time_to_string                  (int64_t                 msecs,
                                                           TotemTimeFlag           flags);
void         valent_sms_avatar_from_contact               (AdwAvatar              *avatar,
                                                           EContact               *contact);
void         valent_contact_store_lookup_contact          (ValentContactStore     *store,
                                                           const char             *number,
                                                           GCancellable           *cancellable,
                                                           GAsyncReadyCallback     callback,
                                                           gpointer                user_data);
EContact   * valent_contact_store_lookup_contact_finish   (ValentContactStore     *store,
                                                           GAsyncResult           *result,
                                                           GError                **error);
void         valent_messages_adapter_lookup_thread        (ValentMessagesAdapter  *adapter,
                                                           const char * const     *participants,
                                                           GCancellable           *cancellable,
                                                           GAsyncReadyCallback     callback,
                                                           gpointer                user_data);
GListModel * valent_messages_adapter_lookup_thread_finish (ValentMessagesAdapter  *adapter,
                                                           GAsyncResult           *result,
                                                           GError                **error);
void         valent_messages_adapter_search               (ValentMessagesAdapter  *adapter,
                                                           const char             *query,
                                                           GCancellable           *cancellable,
                                                           GAsyncReadyCallback     callback,
                                                           gpointer                user_data);
GListModel * valent_messages_adapter_search_finish        (ValentMessagesAdapter  *adapter,
                                                           GAsyncResult           *result,
                                                           GError                **error);

G_END_DECLS
