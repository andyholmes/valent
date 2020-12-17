// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-test-notifications"

#include "config.h"

#include <sys/time.h>
#include <libvalent-notifications.h>

#include "valent-test-notification-source.h"


struct _ValentTestNotificationSource
{
  ValentNotificationSource  parent_instance;
};

G_DEFINE_TYPE (ValentTestNotificationSource, valent_test_notification_source, VALENT_TYPE_NOTIFICATION_SOURCE)


static ValentNotificationSource *test_instance = NULL;

/*
 * ValentNotificationSource
 */
static void
valent_test_notification_source_load_async (ValentNotificationSource *source,
                                            GCancellable             *cancellable,
                                            GAsyncReadyCallback       callback,
                                            gpointer                  user_data)
{
  g_autoptr (GTask) task = NULL;

  g_assert (VALENT_IS_TEST_NOTIFICATION_SOURCE (source));

  task = g_task_new (source, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_test_notification_source_load_async);
  g_task_return_boolean (task, TRUE);
}

/*
 * GObject
 */
static void
valent_test_notification_source_class_init (ValentTestNotificationSourceClass *klass)
{
  ValentNotificationSourceClass *source_class = VALENT_NOTIFICATION_SOURCE_CLASS (klass);

  source_class->load_async = valent_test_notification_source_load_async;
}

static void
valent_test_notification_source_init (ValentTestNotificationSource *self)
{
  if (test_instance == NULL)
    {
      test_instance = VALENT_NOTIFICATION_SOURCE (self);
      g_object_add_weak_pointer (G_OBJECT (test_instance),
                                 (gpointer)&test_instance);
    }
}

/**
 * valent_test_notification_source_get_instance:
 *
 * Get the #ValentTestNoitificationSource instance.
 *
 * Returns: (transfer none) (nullable): a #ValentNotificationSource
 */
ValentNotificationSource *
valent_test_notification_source_get_instance (void)
{
  return test_instance;
}

