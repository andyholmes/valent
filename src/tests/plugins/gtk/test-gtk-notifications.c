// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <locale.h>

#include <gio/gio.h>
#include <gtk/gtk.h>
#include <libvalent-core.h>
#include <libvalent-notifications.h>
#include <libvalent-test.h>


typedef struct
{
  ValentNotifications *notifications;
  GDBusConnection     *connection;
  GMainLoop           *loop;

  ValentNotification  *notification;
  char                *notification_id;
  unsigned int         notification_nid;
} GtkNotificationsFixture;

static void
on_notification_added (ValentNotifications     *notifications,
                       ValentNotification      *notification,
                       GtkNotificationsFixture *fixture)
{
  g_set_object (&fixture->notification, notification);
  g_main_loop_quit (fixture->loop);
}

static void
on_notification_removed (ValentNotifications     *notifications,
                         const char              *id,
                         GtkNotificationsFixture *fixture)
{
  g_clear_pointer (&fixture->notification_id, g_free);
  fixture->notification_id = g_strdup (id);
  g_main_loop_quit (fixture->loop);
}

static void
gtk_notifications_fixture_set_up (GtkNotificationsFixture *fixture,
                                  gconstpointer            user_data)
{
  g_autoptr (GSettings) settings = NULL;

  /* Disable the mock plugin */
  settings = valent_component_create_settings ("notifications", "mock");
  g_settings_set_boolean (settings, "enabled", FALSE);

  fixture->connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
  fixture->loop = g_main_loop_new (NULL, FALSE);
  fixture->notifications = valent_notifications_get_default ();
}

static void
gtk_notifications_fixture_tear_down (GtkNotificationsFixture *fixture,
                                     gconstpointer            user_data)
{
  g_clear_object (&fixture->connection);
  g_clear_pointer (&fixture->loop, g_main_loop_unref);
  g_clear_pointer (&fixture->notification_id, g_free);
  v_assert_finalize_object (fixture->notification);
  v_assert_finalize_object (fixture->notifications);
}



static void
notification_cb (GDBusConnection         *connection,
                 GAsyncResult            *result,
                 GtkNotificationsFixture *fixture)
{
  g_autoptr (GVariant) reply = NULL;
  g_autoptr (GError) error = NULL;

  reply = g_dbus_connection_call_finish (connection, result, &error);
  g_assert_no_error (error);
}

static void
remove_notification (GtkNotificationsFixture *fixture)
{
  g_dbus_connection_call (fixture->connection,
                          "org.gtk.Notifications",
                          "/org/gtk/Notifications",
                          "org.gtk.Notifications",
                          "RemoveNotification",
                          g_variant_new ("(ss)", "ca.andyholmes.Valent.Test",
                                         "test-notification"),
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          NULL,
                          (GAsyncReadyCallback)notification_cb,
                          fixture);
}

static void
add_notification (GtkNotificationsFixture *fixture)
{
  GVariantBuilder notification_builder;
  g_autoptr (GIcon) icon = NULL;
  g_autoptr (GVariant) iconv = NULL;

  icon = g_themed_icon_new ("dialog-information-symbolic");
  iconv = g_icon_serialize (icon);

  g_variant_builder_init (&notification_builder, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add (&notification_builder, "{sv}",
                         "icon", iconv);
  g_variant_builder_add (&notification_builder, "{sv}",
                         "title", g_variant_new_string ("Test Title"));
  g_variant_builder_add (&notification_builder, "{sv}",
                         "body", g_variant_new_string ("Test Body"));
  g_variant_builder_add (&notification_builder, "{sv}",
                         "priority", g_variant_new_string ("urgent"));

  g_dbus_connection_call (fixture->connection,
                          "org.gtk.Notifications",
                          "/org/gtk/Notifications",
                          "org.gtk.Notifications",
                          "AddNotification",
                          g_variant_new ("(ssa{sv})",
                                         "ca.andyholmes.Valent.Test",
                                         "test-notification",
                                         &notification_builder),
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          NULL,
                          (GAsyncReadyCallback)notification_cb,
                          fixture);
}

static gboolean
on_timeout (gpointer data)
{
  GtkNotificationsFixture *fixture = data;

  g_main_loop_quit (fixture->loop);

  return G_SOURCE_REMOVE;
}

static void
test_gtk_notifications_source (GtkNotificationsFixture *fixture,
                               gconstpointer            user_data)
{
  g_autoptr (GIcon) cmp_icon = NULL;
  g_autoptr (GIcon) icon = NULL;
  g_autofree char *id = NULL;
  g_autofree char *application = NULL;
  g_autofree char *title = NULL;
  g_autofree char *body = NULL;
  GNotificationPriority priority;

  /* Wait a bit longer for valent_notifications_adapter_load_async() to resolve
   * NOTE: this is longer than most tests due to the chained async functions
   *       being called in ValentGtkNotifications.
   */
  g_timeout_add_seconds (1, on_timeout, fixture);
  g_main_loop_run (fixture->loop);

  g_signal_connect (fixture->notifications,
                    "notification-added",
                    G_CALLBACK (on_notification_added),
                    fixture);
  g_signal_connect (fixture->notifications,
                    "notification-removed",
                    G_CALLBACK (on_notification_removed),
                    fixture);

  /* Add notification */
  add_notification (fixture);
  g_main_loop_run (fixture->loop);
  g_assert_true (VALENT_IS_NOTIFICATION (fixture->notification));

  /* Test Notification */
  cmp_icon = g_themed_icon_new ("dialog-information-symbolic");
  g_object_get (fixture->notification,
                "id",          &id,
                "application", &application,
                "title",       &title,
                "body",        &body,
                "icon",        &icon,
                "priority",    &priority,
                NULL);

  /* g_assert_cmpstr (application, ==, "Test Application"); */
  g_assert_cmpstr (title, ==, "Test Title");
  g_assert_cmpstr (body, ==, "Test Body");
  g_assert_true (g_icon_equal (icon, cmp_icon));
  g_assert_cmpuint (priority, ==, G_NOTIFICATION_PRIORITY_URGENT);

  /* Remove Notification */
  remove_notification (fixture);
  g_main_loop_run (fixture->loop);
  g_assert_cmpstr (id, ==, fixture->notification_id);

  g_signal_handlers_disconnect_by_data (fixture->notifications, fixture);
}

int
main (int   argc,
      char *argv[])
{
  valent_test_init (&argc, &argv, NULL);

  /* NOTE: This suite will time out if valent_ui_test_init() is used */
  gtk_disable_setlocale ();
  setlocale (LC_ALL, "en_US.UTF-8");
  gtk_init ();

  g_test_add ("/plugins/gtk/notifications",
              GtkNotificationsFixture, NULL,
              gtk_notifications_fixture_set_up,
              test_gtk_notifications_source,
              gtk_notifications_fixture_tear_down);

  return g_test_run ();
}
