// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_NOTIFICATIONS_INSIDE) && !defined (VALENT_NOTIFICATIONS_COMPILATION)
# error "Only <libvalent-notifications.h> can be included directly."
#endif

#include <libvalent-core.h>

#include "valent-notification.h"

G_BEGIN_DECLS

#define VALENT_TYPE_NOTIFICATIONS_ADAPTER (valent_notifications_adapter_get_type())

VALENT_AVAILABLE_IN_1_0
G_DECLARE_DERIVABLE_TYPE (ValentNotificationsAdapter, valent_notifications_adapter, VALENT, NOTIFICATIONS_ADAPTER, GObject)

struct _ValentNotificationsAdapterClass
{
  GObjectClass   parent_class;

  /* virtual functions */
  void           (*add_notification)     (ValentNotificationsAdapter  *adapter,
                                          ValentNotification          *notification);
  void           (*remove_notification)  (ValentNotificationsAdapter  *adapter,
                                          const char                  *id);
  void           (*load_async)           (ValentNotificationsAdapter  *adapter,
                                          GCancellable                *cancellable,
                                          GAsyncReadyCallback          callback,
                                          gpointer                     user_data);
  gboolean       (*load_finish)          (ValentNotificationsAdapter  *adapter,
                                          GAsyncResult                *result,
                                          GError                     **error);

  /* signals */
  void           (*notification_added)   (ValentNotificationsAdapter *adapter,
                                          ValentNotification         *notification);
  void           (*notification_removed) (ValentNotificationsAdapter *adapter,
                                          const char                 *id);
};

VALENT_AVAILABLE_IN_1_0
void       valent_notifications_adapter_emit_notification_added   (ValentNotificationsAdapter  *adapter,
                                                                   ValentNotification          *notification);
VALENT_AVAILABLE_IN_1_0
void       valent_notifications_adapter_emit_notification_removed (ValentNotificationsAdapter  *adapter,
                                                                   const char                  *id);
VALENT_AVAILABLE_IN_1_0
void       valent_notifications_adapter_add_notification          (ValentNotificationsAdapter  *adapter,
                                                                   ValentNotification          *notification);
VALENT_AVAILABLE_IN_1_0
void       valent_notifications_adapter_remove_notification       (ValentNotificationsAdapter  *adapter,
                                                                   const char                  *id);
VALENT_AVAILABLE_IN_1_0
void       valent_notifications_adapter_load_async                (ValentNotificationsAdapter  *adapter,
                                                                   GCancellable                *cancellable,
                                                                   GAsyncReadyCallback          callback,
                                                                   gpointer                     user_data);
VALENT_AVAILABLE_IN_1_0
gboolean   valent_notifications_adapter_load_finish               (ValentNotificationsAdapter  *adapter,
                                                                   GAsyncResult                *result,
                                                                   GError                     **error);

G_END_DECLS

