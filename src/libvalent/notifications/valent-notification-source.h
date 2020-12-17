// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_NOTIFICATIONS_INSIDE) && !defined (VALENT_NOTIFICATIONS_COMPILATION)
# error "Only <libvalent-notifications.h> can be included directly."
#endif

#include <glib-object.h>

#include "valent-notification.h"

G_BEGIN_DECLS

#define VALENT_TYPE_NOTIFICATION_SOURCE (valent_notification_source_get_type())

G_DECLARE_DERIVABLE_TYPE (ValentNotificationSource, valent_notification_source, VALENT, NOTIFICATION_SOURCE, GObject)

struct _ValentNotificationSourceClass
{
  GObjectClass   parent_class;

  /* virtual functions */
  void           (*add_notification)     (ValentNotificationSource  *source,
                                          ValentNotification        *notification);
  void           (*remove_notification)  (ValentNotificationSource  *source,
                                          const char                *id);
  void           (*load_async)           (ValentNotificationSource  *source,
                                          GCancellable              *cancellable,
                                          GAsyncReadyCallback        callback,
                                          gpointer                   user_data);
  gboolean       (*load_finish)          (ValentNotificationSource  *source,
                                          GAsyncResult              *result,
                                          GError                   **error);

  /* signals */
  void           (*notification_added)   (ValentNotificationSource *source,
                                          ValentNotification       *notification);
  void           (*notification_removed) (ValentNotificationSource *source,
                                          const char               *id);
};

void       valent_notification_source_emit_notification_added   (ValentNotificationSource  *source,
                                                                 ValentNotification        *notification);
void       valent_notification_source_emit_notification_removed (ValentNotificationSource  *source,
                                                                 const char                *id);
void       valent_notification_source_add_notification          (ValentNotificationSource  *source,
                                                                 ValentNotification        *notification);
void       valent_notification_source_remove_notification       (ValentNotificationSource  *source,
                                                                 const char                *id);
void       valent_notification_source_load_async                (ValentNotificationSource  *source,
                                                                 GCancellable              *cancellable,
                                                                 GAsyncReadyCallback        callback,
                                                                 gpointer                   user_data);
gboolean   valent_notification_source_load_finish               (ValentNotificationSource  *source,
                                                                 GAsyncResult              *result,
                                                                 GError                   **error);

G_END_DECLS

