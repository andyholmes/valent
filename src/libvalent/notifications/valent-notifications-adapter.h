// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_INSIDE) && !defined (VALENT_COMPILATION)
# error "Only <valent.h> can be included directly."
#endif

#include "../core/valent-extension.h"
#include "valent-notification.h"

G_BEGIN_DECLS

#define VALENT_TYPE_NOTIFICATIONS_ADAPTER (valent_notifications_adapter_get_type())

VALENT_AVAILABLE_IN_1_0
G_DECLARE_DERIVABLE_TYPE (ValentNotificationsAdapter, valent_notifications_adapter, VALENT, NOTIFICATIONS_ADAPTER, ValentExtension)

struct _ValentNotificationsAdapterClass
{
  ValentExtensionClass   parent_class;

  /* virtual functions */
  void                   (*add_notification)     (ValentNotificationsAdapter  *adapter,
                                                  ValentNotification          *notification);
  void                   (*remove_notification)  (ValentNotificationsAdapter  *adapter,
                                                  const char                  *id);

  /* signals */
  void                   (*notification_added)   (ValentNotificationsAdapter  *adapter,
                                                  ValentNotification          *notification);
  void                   (*notification_removed) (ValentNotificationsAdapter  *adapter,
                                                  const char                  *id);

  /*< private >*/
  gpointer               padding[8];
};

VALENT_AVAILABLE_IN_1_0
void       valent_notifications_adapter_notification_added   (ValentNotificationsAdapter  *adapter,
                                                              ValentNotification          *notification);
VALENT_AVAILABLE_IN_1_0
void       valent_notifications_adapter_notification_removed (ValentNotificationsAdapter  *adapter,
                                                              const char                  *id);
VALENT_AVAILABLE_IN_1_0
void       valent_notifications_adapter_add_notification     (ValentNotificationsAdapter  *adapter,
                                                              ValentNotification          *notification);
VALENT_AVAILABLE_IN_1_0
void       valent_notifications_adapter_remove_notification  (ValentNotificationsAdapter  *adapter,
                                                              const char                  *id);

G_END_DECLS

