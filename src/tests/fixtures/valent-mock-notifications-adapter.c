// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-mock-notifications"

#include "config.h"

#include <libvalent-notifications.h>

#include "valent-mock-notifications-adapter.h"


struct _ValentMockNotificationsAdapter
{
  ValentNotificationsAdapter  parent_instance;
};

G_DEFINE_TYPE (ValentMockNotificationsAdapter, valent_mock_notifications_adapter, VALENT_TYPE_NOTIFICATIONS_ADAPTER)


static ValentNotificationsAdapter *test_instance = NULL;

/*
 * ValentNotificationsAdapter
 */
static void
valent_mock_notifications_adapter_load_async (ValentNotificationsAdapter *adapter,
                                              GCancellable               *cancellable,
                                              GAsyncReadyCallback         callback,
                                              gpointer                    user_data)
{
  g_autoptr (GTask) task = NULL;

  g_assert (VALENT_IS_MOCK_NOTIFICATIONS_ADAPTER (adapter));

  task = g_task_new (adapter, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_mock_notifications_adapter_load_async);
  g_task_return_boolean (task, TRUE);
}

/*
 * GObject
 */
static void
valent_mock_notifications_adapter_class_init (ValentMockNotificationsAdapterClass *klass)
{
  ValentNotificationsAdapterClass *adapter_class = VALENT_NOTIFICATIONS_ADAPTER_CLASS (klass);

  adapter_class->load_async = valent_mock_notifications_adapter_load_async;
}

static void
valent_mock_notifications_adapter_init (ValentMockNotificationsAdapter *self)
{
  if (test_instance == NULL)
    {
      test_instance = VALENT_NOTIFICATIONS_ADAPTER (self);
      g_object_add_weak_pointer (G_OBJECT (test_instance),
                                 (gpointer)&test_instance);
    }
}

/**
 * valent_mock_notifications_adapter_get_instance:
 *
 * Get the #ValentMockNotificationsAdapter instance.
 *
 * Returns: (transfer none) (nullable): a #ValentNotificationsAdapter
 */
ValentNotificationsAdapter *
valent_mock_notifications_adapter_get_instance (void)
{
  return test_instance;
}

