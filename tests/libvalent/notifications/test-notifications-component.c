// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>
#include <valent.h>
#include <libvalent-test.h>


typedef struct
{
} NotificationsComponentFixture;

static void
notifications_component_fixture_set_up (NotificationsComponentFixture *fixture,
                                        gconstpointer                  user_data)
{
}

static void
notifications_component_fixture_tear_down (NotificationsComponentFixture *fixture,
                                           gconstpointer                  user_data)
{
}

static void
test_notifications_component_notification (NotificationsComponentFixture *fixture,
                                           gconstpointer                  user_data)
{
  g_autoptr (ValentNotification) notification = NULL;
  g_autoptr (ValentNotification) notification_out = NULL;
  g_autoptr (GVariant) serialized = NULL;
  g_autoptr (GIcon) icon = NULL;
  g_autoptr (GIcon) icon_out = NULL;
  g_autofree char *id = NULL;
  g_autofree char *application = NULL;
  g_autofree char *title = NULL;
  g_autofree char *body = NULL;
  GNotificationPriority priority;
  int64_t time, time_out;

  VALENT_TEST_CHECK ("Notification can be constructed");
  notification = g_object_new (VALENT_TYPE_NOTIFICATION,
                               "id",    "test-id",
                               "title", "Test Title",
                               NULL);

  VALENT_TEST_CHECK ("GObject properties function correctly");
  icon = g_themed_icon_new ("dialog-information-symbolic");
  time = valent_timestamp_ms ();
  g_object_set (notification,
                "id",          "test-id",
                "application", "Test Application",
                "title",       "Test Title",
                "body",        "Test Body",
                "icon",        icon,
                "priority",    G_NOTIFICATION_PRIORITY_HIGH,
                "time",        time,
                NULL);

  g_object_get (notification,
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

  VALENT_TEST_CHECK ("Notifications can hold action buttons");
  valent_notification_add_button (notification,
                                  "Button 1",
                                  "foo.bar::baz",
                                  NULL);

  VALENT_TEST_CHECK ("Notifications can be serialized and deserialized");
  serialized = valent_notification_serialize (notification);
  notification_out = valent_notification_deserialize (serialized);

  VALENT_TEST_CHECK ("Notifications can be hashed and compared");
  g_assert_true (valent_notification_equal (notification, notification_out));
  g_assert_cmpuint (valent_notification_hash (notification), ==, valent_notification_hash (notification_out));
}

static void
on_items_changed (GListModel          *list,
                  unsigned int         position,
                  unsigned int         removed,
                  unsigned int         added,
                  ValentNotification **notification_out)
{
  if (removed)
    {
      g_clear_object (notification_out);
    }

  if (added)
    {
      g_clear_object (notification_out);
      *notification_out = g_list_model_get_item (list, position);
    }
}

static void
test_notifications_component_adapter (NotificationsComponentFixture *fixture,
                                      gconstpointer                  user_data)
{
  PeasEngine *engine;
  PeasPluginInfo *plugin_info;
  g_autoptr (ValentContext) context = NULL;
  g_autoptr (GObject) adapter = NULL;
  g_autoptr (ValentNotification) notification = NULL;
  g_autoptr (ValentNotification) notification_out = NULL;
  unsigned int n_items = 0;

  engine = valent_get_plugin_engine ();
  plugin_info = peas_engine_get_plugin_info (engine, "mock");
  context = valent_context_new (NULL, "plugin", "mock");

  VALENT_TEST_CHECK ("Adapter can be constructed");
  adapter = peas_engine_create_extension (engine,
                                          plugin_info,
                                          VALENT_TYPE_NOTIFICATIONS_ADAPTER,
                                          "iri",     "urn:valent:notifications:mock",
                                          "source",  NULL,
                                          "context", context,
                                          NULL);

  g_signal_connect (adapter,
                    "items-changed",
                    G_CALLBACK (on_items_changed),
                    &notification_out);

  VALENT_TEST_CHECK ("Notification can be constructed");
  notification = g_object_new (VALENT_TYPE_NOTIFICATION,
                               "id",    "test-id",
                               "title", "Test Title",
                               NULL);

  VALENT_TEST_CHECK ("Adapter adds notifications");
  valent_notifications_adapter_notification_added (VALENT_NOTIFICATIONS_ADAPTER (adapter),
                                                   notification);
  g_assert_true (valent_notification_equal (notification, notification_out));

  VALENT_TEST_CHECK ("Adapter implements GListModel correctly");
  g_assert_true (G_LIST_MODEL (adapter));
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (adapter)), >, 0);
  g_assert_true (g_list_model_get_item_type (G_LIST_MODEL (adapter)) == VALENT_TYPE_NOTIFICATION);

  n_items = g_list_model_get_n_items (G_LIST_MODEL (adapter));
  for (unsigned int i = 0; i < n_items; i++)
    {
      g_autoptr (GListModel) item = g_list_model_get_item (G_LIST_MODEL (adapter), i);
      g_assert_true (VALENT_IS_NOTIFICATION (item));
    }

  VALENT_TEST_CHECK ("Adapter removes notifications");
  valent_notifications_adapter_notification_removed (VALENT_NOTIFICATIONS_ADAPTER (adapter),
                                                     notification);
  g_assert_null (notification_out);

  g_signal_handlers_disconnect_by_data (adapter, &notification_out);
}

static void
test_notifications_component_self (NotificationsComponentFixture *fixture,
                                   gconstpointer                  user_data)
{
  ValentNotifications *notifications = valent_notifications_get_default ();
  unsigned int n_items = 0;
  GVariant *applications;

  VALENT_TEST_CHECK ("Component implements GListModel correctly");
  g_assert_true (G_LIST_MODEL (notifications));
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (notifications)), >, 0);
  g_assert_true (g_list_model_get_item_type (G_LIST_MODEL (notifications)) == VALENT_TYPE_NOTIFICATIONS_ADAPTER);

  n_items = g_list_model_get_n_items (G_LIST_MODEL (notifications));
  for (unsigned int i = 0; i < n_items; i++)
    {
      g_autoptr (GObject) item = NULL;

      item = g_list_model_get_item (G_LIST_MODEL (notifications), i);
      g_assert_true (VALENT_IS_NOTIFICATIONS_ADAPTER (item));
    }

  VALENT_TEST_CHECK ("Component aggregates application sources");
  applications = valent_notifications_get_applications (notifications);
  g_assert_nonnull (applications);
}

int
main (int   argc,
      char *argv[])
{
  valent_test_init (&argc, &argv, NULL);

  g_test_add ("/libvalent/notifications/notification",
              NotificationsComponentFixture, NULL,
              notifications_component_fixture_set_up,
              test_notifications_component_notification,
              notifications_component_fixture_tear_down);

  g_test_add ("/libvalent/notifications/adapter",
              NotificationsComponentFixture, NULL,
              notifications_component_fixture_set_up,
              test_notifications_component_adapter,
              notifications_component_fixture_tear_down);

  g_test_add ("/libvalent/notifications/self",
              NotificationsComponentFixture, NULL,
              notifications_component_fixture_set_up,
              test_notifications_component_self,
              notifications_component_fixture_tear_down);

  return g_test_run ();
}
