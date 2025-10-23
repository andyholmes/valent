// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>
#include <valent.h>
#include <libvalent-test.h>


typedef struct
{
  ValentNotifications        *notifications;
  ValentNotificationsAdapter *adapter;
  ValentNotification         *notification;
  gpointer                    data;
} NotificationsComponentFixture;

static void
on_notification_added (GObject                       *object,
                       ValentNotification            *notification,
                       NotificationsComponentFixture *fixture)
{
  fixture->data = object;
}

static void
on_notification_removed (GObject                       *object,
                         const char                    *id,
                         NotificationsComponentFixture *fixture)
{
  fixture->data = object;
}

static void
notifications_component_fixture_set_up (NotificationsComponentFixture *fixture,
                                        gconstpointer                  user_data)
{
  fixture->notifications = valent_notifications_get_default ();
  fixture->adapter = valent_test_await_adapter (fixture->notifications);
  fixture->notification = g_object_new (VALENT_TYPE_NOTIFICATION,
                                        "title", "Test Title",
                                        "id",    "test-id",
                                        NULL);

  g_object_ref (fixture->adapter);
}

static void
notifications_component_fixture_tear_down (NotificationsComponentFixture *fixture,
                                           gconstpointer                  user_data)
{
  v_await_finalize_object (fixture->notifications);
  v_await_finalize_object (fixture->adapter);
  v_await_finalize_object (fixture->notification);
}

static void
test_notifications_component_adapter (NotificationsComponentFixture *fixture,
                                      gconstpointer                  user_data)
{
  g_signal_connect (fixture->adapter,
                    "notification-added",
                    G_CALLBACK (on_notification_added),
                    fixture);
  g_signal_connect (fixture->adapter,
                    "notification-removed",
                    G_CALLBACK (on_notification_removed),
                    fixture);

  /* Signals */
  valent_notifications_adapter_notification_added (fixture->adapter, fixture->notification);
  g_assert_true (fixture->data == fixture->adapter);
  fixture->data = NULL;

  valent_notifications_adapter_notification_removed (fixture->adapter, "test-id");
  g_assert_true (fixture->data == fixture->adapter);
  fixture->data = NULL;

  g_signal_handlers_disconnect_by_data (fixture->adapter, fixture);
}

static void
test_notifications_component_notification (NotificationsComponentFixture *fixture,
                                           gconstpointer                  user_data)
{
  g_autoptr (ValentNotification) notification = NULL;
  g_autoptr (GVariant) serialized = NULL;
  g_autoptr (GIcon) icon = NULL;
  g_autoptr (GIcon) icon_out = NULL;
  g_autofree char *id = NULL;
  g_autofree char *application = NULL;
  g_autofree char *title = NULL;
  g_autofree char *body = NULL;
  GNotificationPriority priority;
  int64_t time, time_out;

  g_signal_connect (fixture->adapter,
                    "notification-added",
                    G_CALLBACK (on_notification_added),
                    fixture);
  g_signal_connect (fixture->adapter,
                    "notification-removed",
                    G_CALLBACK (on_notification_removed),
                    fixture);

  /* Add Notification */
  icon = g_themed_icon_new ("dialog-information-symbolic");
  time = valent_timestamp_ms ();
  g_object_set (fixture->notification,
                "id",          "test-id",
                "application", "Test Application",
                "title",       "Test Title",
                "body",        "Test Body",
                "icon",        icon,
                "priority",    G_NOTIFICATION_PRIORITY_HIGH,
                "time",        time,
                NULL);

  valent_notification_set_default_action (fixture->notification, "foo.bar::baz", NULL);
  valent_notification_add_button (fixture->notification, "Button 1", "foo.bar::baz", NULL);


  valent_notifications_adapter_notification_added (fixture->adapter, fixture->notification);
  g_assert_true (fixture->data == fixture->adapter);
  fixture->data = NULL;

  /* Test Notification */
  g_object_get (fixture->notification,
                "id",          &id,
                "application", &application,
                "title",       &title,
                "body",        &body,
                "icon",        &icon_out,
                "priority",    &priority,
                "time",        &time_out,
                NULL);

  g_assert_cmpstr (id, ==, "test-id");
  g_assert_cmpstr (application, ==, "Test Application");
  g_assert_cmpstr (title, ==, "Test Title");
  g_assert_cmpstr (body, ==, "Test Body");
  g_assert_true (g_icon_equal (icon, icon_out));
  g_assert_cmpuint (priority, ==, G_NOTIFICATION_PRIORITY_HIGH);
  g_assert_cmpint (time, ==, time_out);

  serialized = valent_notification_serialize (fixture->notification);
  notification = valent_notification_deserialize (serialized);

  g_assert_true (valent_notification_equal (fixture->notification, notification));
  g_assert_cmpuint (valent_notification_hash (fixture->notification), ==, valent_notification_hash (notification));

  /* Remove Notification */
  valent_notifications_adapter_notification_removed (fixture->adapter, "test-id");
  g_assert_true (fixture->data == fixture->adapter);
  fixture->data = NULL;

  g_signal_handlers_disconnect_by_data (fixture->adapter, fixture);
}

static void
test_notifications_component_self (NotificationsComponentFixture *fixture,
                                   gconstpointer                  user_data)
{
  g_signal_connect (fixture->notifications,
                    "notification-added",
                    G_CALLBACK (on_notification_added),
                    fixture);
  g_signal_connect (fixture->notifications,
                    "notification-removed",
                    G_CALLBACK (on_notification_removed),
                    fixture);

  /* Add notification */
  valent_notifications_adapter_notification_added (fixture->adapter, fixture->notification);
  g_assert_true (fixture->data == fixture->notifications);
  fixture->data = NULL;

  /* Remove notification */
  valent_notifications_adapter_notification_removed (fixture->adapter, "test-id");
  g_assert_true (fixture->data == fixture->notifications);
  fixture->data = NULL;

  g_signal_handlers_disconnect_by_data (fixture->adapter, fixture->notifications);
}

int
main (int   argc,
      char *argv[])
{
  valent_test_init (&argc, &argv, NULL);

  g_test_add ("/libvalent/notifications/adapter",
              NotificationsComponentFixture, NULL,
              notifications_component_fixture_set_up,
              test_notifications_component_adapter,
              notifications_component_fixture_tear_down);

  g_test_add ("/libvalent/notifications/notification",
              NotificationsComponentFixture, NULL,
              notifications_component_fixture_set_up,
              test_notifications_component_notification,
              notifications_component_fixture_tear_down);

  g_test_add ("/libvalent/notifications/self",
              NotificationsComponentFixture, NULL,
              notifications_component_fixture_set_up,
              test_notifications_component_self,
              notifications_component_fixture_tear_down);

  return g_test_run ();
}
