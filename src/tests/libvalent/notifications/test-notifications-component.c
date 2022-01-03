// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>
#include <libvalent-core.h>
#include <libvalent-notifications.h>
#include <libvalent-test.h>


typedef struct
{
  ValentNotifications *notifications;
  ValentNotification  *notification;
  gpointer             data;
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
  fixture->notification = g_object_new (VALENT_TYPE_NOTIFICATION,
                                        "title", "Test Title",
                                        "id",    "test-id",
                                        NULL);
}

static void
notifications_component_fixture_tear_down (NotificationsComponentFixture *fixture,
                                           gconstpointer                  user_data)
{
  g_assert_finalize_object (fixture->notifications);
  g_assert_finalize_object (fixture->notification);
}

static void
test_notifications_component_provider (NotificationsComponentFixture *fixture,
                                       gconstpointer                  user_data)
{
  ValentNotificationSource *source;
  PeasPluginInfo *info;

  while ((source = valent_mock_notification_source_get_instance ()) == NULL)
    continue;

  /* Wait a bit longer for valent_notification_source_load_async() to resolve */
  while (g_main_context_iteration (NULL, FALSE))
    continue;

  g_signal_connect (source,
                    "notification-added",
                    G_CALLBACK (on_notification_added),
                    fixture);
  g_signal_connect (source,
                    "notification-removed",
                    G_CALLBACK (on_notification_removed),
                    fixture);

  /* Properties */
  g_object_get (source,
                "plugin-info", &info,
                NULL);
  g_assert_nonnull (info);
  g_boxed_free (PEAS_TYPE_PLUGIN_INFO, info);

  /* Signals */
  valent_notification_source_emit_notification_added (source, fixture->notification);
  g_assert_true (fixture->data == source);
  fixture->data = NULL;

  valent_notification_source_emit_notification_removed (source, "test-id");
  g_assert_true (fixture->data == source);
  fixture->data = NULL;

  g_signal_handlers_disconnect_by_data (source, fixture);
}

static void
test_notifications_component_notification (NotificationsComponentFixture *fixture,
                                           gconstpointer                  user_data)
{
  ValentNotificationSource *source;
  g_autoptr (GIcon) icon = NULL;
  char *id, *application, *title, *body;
  GIcon *nicon;
  GNotificationPriority priority;
  gint64 time, ntime;

  while ((source = valent_mock_notification_source_get_instance ()) == NULL)
    continue;

  /* Wait a bit longer for valent_notification_source_load_async() to resolve */
  while (g_main_context_iteration (NULL, FALSE))
    continue;

  g_signal_connect (source,
                    "notification-added",
                    G_CALLBACK (on_notification_added),
                    fixture);
  g_signal_connect (source,
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
                "action",      "foo.bar::baz",
                "time",        time,
                NULL);

  valent_notification_add_button (fixture->notification, "Button 1", "foo.bar::baz");


  valent_notification_source_emit_notification_added (source, fixture->notification);
  g_assert_true (fixture->data == source);
  fixture->data = NULL;

  /* Test Notification */
  g_object_get (fixture->notification,
                "id",          &id,
                "application", &application,
                "title",       &title,
                "body",        &body,
                "icon",        &nicon,
                "priority",    &priority,
                "time",        &ntime,
                NULL);

  g_assert_cmpstr (id, ==, "test-id");
  g_assert_cmpstr (application, ==, "Test Application");
  g_assert_cmpstr (title, ==, "Test Title");
  g_assert_cmpstr (body, ==, "Test Body");
  g_assert_true (g_icon_equal (icon, nicon));
  g_assert_cmpuint (priority, ==, G_NOTIFICATION_PRIORITY_HIGH);
  g_assert_cmpint (time, ==, ntime);

  g_free (id);
  g_free (application);
  g_free (title);
  g_free (body);
  g_object_unref (nicon);

  g_autoptr (GVariant) variant = NULL;
  g_autoptr (ValentNotification) notification = NULL;

  variant = valent_notification_serialize (fixture->notification);
  notification = valent_notification_deserialize (variant);

  /* Remove Notification */
  valent_notification_source_emit_notification_removed (source, "test-id");
  g_assert_true (fixture->data == source);
  fixture->data = NULL;

  g_signal_handlers_disconnect_by_data (source, fixture);
}

static void
test_notifications_component_self (NotificationsComponentFixture *fixture,
                                   gconstpointer                  user_data)
{
  ValentNotificationSource *source;

  while ((source = valent_mock_notification_source_get_instance ()) == NULL)
    continue;

  /* Wait a bit longer for valent_notification_source_load_async() to resolve */
  while (g_main_context_iteration (NULL, FALSE))
    continue;

  g_signal_connect (fixture->notifications,
                    "notification-added",
                    G_CALLBACK (on_notification_added),
                    fixture);
  g_signal_connect (fixture->notifications,
                    "notification-removed",
                    G_CALLBACK (on_notification_removed),
                    fixture);

  /* Add notification */
  valent_notification_source_emit_notification_added (source, fixture->notification);
  g_assert_true (fixture->data == fixture->notifications);
  fixture->data = NULL;

  /* Remove notification */
  valent_notification_source_emit_notification_removed (source, "test-id");
  g_assert_true (fixture->data == fixture->notifications);
  fixture->data = NULL;

  g_signal_handlers_disconnect_by_data (source, fixture->notifications);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, G_TEST_OPTION_ISOLATE_DIRS, NULL);

  g_test_add ("/components/notifications/provider",
              NotificationsComponentFixture, NULL,
              notifications_component_fixture_set_up,
              test_notifications_component_provider,
              notifications_component_fixture_tear_down);

  g_test_add ("/components/notifications/notification",
              NotificationsComponentFixture, NULL,
              notifications_component_fixture_set_up,
              test_notifications_component_notification,
              notifications_component_fixture_tear_down);

  g_test_add ("/components/notifications/self",
              NotificationsComponentFixture, NULL,
              notifications_component_fixture_set_up,
              test_notifications_component_self,
              notifications_component_fixture_tear_down);

  return g_test_run ();
}
