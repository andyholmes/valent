// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-mock-notifications"

#include "config.h"

#include <valent.h>

#include "valent-mock-notifications-adapter.h"


struct _ValentMockNotificationsAdapter
{
  ValentNotificationsAdapter  parent_instance;
};

G_DEFINE_FINAL_TYPE (ValentMockNotificationsAdapter, valent_mock_notifications_adapter, VALENT_TYPE_NOTIFICATIONS_ADAPTER)


/*
 * GObject
 */
static void
valent_mock_notifications_adapter_class_init (ValentMockNotificationsAdapterClass *klass)
{
}

static void
valent_mock_notifications_adapter_init (ValentMockNotificationsAdapter *self)
{
}

