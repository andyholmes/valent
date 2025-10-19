// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include "config.h"

#include <gio/gio.h>
#include <valent.h>
#include <libvalent-test.h>

#ifdef HAVE_GLYCIN
#include <glycin.h>
#endif /* HAVE_GLYCIN */

typedef struct
{
  ValentNotifications *notifications;
  GDBusConnection     *connection;

  unsigned int         notification_nid;
} FdoNotificationsFixture;

static void
on_notification_added (ValentNotifications *notifications,
                       ValentNotification  *notification,
                       ValentNotification  **notification_out)
{
  *notification_out = g_object_ref (notification);
}

static void
on_notification_removed (ValentNotifications  *notifications,
                         const char           *id,
                         char                **notification_id)
{
  *notification_id = g_strdup (id);
}

static void
fdo_notifications_fixture_set_up (FdoNotificationsFixture *fixture,
                                  gconstpointer            user_data)
{
  g_autoptr (GSettings) settings = NULL;

  /* Disable the mock plugin */
  settings = valent_test_mock_settings ("notifications");
  g_settings_set_boolean (settings, "enabled", FALSE);

  fixture->connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
  fixture->notifications = valent_notifications_get_default ();
}

static void
fdo_notifications_fixture_tear_down (FdoNotificationsFixture *fixture,
                                     gconstpointer            user_data)
{
  g_clear_object (&fixture->connection);
  v_await_finalize_object (fixture->notifications);
}

static void
close_notification_cb (GDBusConnection         *connection,
                       GAsyncResult            *result,
                       FdoNotificationsFixture *fixture)
{
  g_autoptr (GVariant) reply = NULL;
  g_autoptr (GError) error = NULL;

  reply = g_dbus_connection_call_finish (connection, result, &error);
  g_assert_no_error (error);
}

static void
close_notification (FdoNotificationsFixture *fixture)
{
  g_dbus_connection_call (fixture->connection,
                          "org.freedesktop.Notifications",
                          "/org/freedesktop/Notifications",
                          "org.freedesktop.Notifications",
                          "CloseNotification",
                          g_variant_new ("(u)", fixture->notification_nid),
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          NULL,
                          (GAsyncReadyCallback)close_notification_cb,
                          fixture);
}

static void
send_notification_cb (GDBusConnection         *connection,
                      GAsyncResult            *result,
                      FdoNotificationsFixture *fixture)
{
  g_autoptr (GVariant) reply = NULL;
  g_autoptr (GError) error = NULL;

  reply = g_dbus_connection_call_finish (connection, result, &error);
  g_assert_no_error (error);

  g_variant_get (reply, "(u)", &fixture->notification_nid);
}

static void
send_notification (FdoNotificationsFixture *fixture,
                   gboolean                 with_pixbuf)
{
  GVariant *notification = NULL;
  GVariantBuilder actions_builder;
  GVariantBuilder hints_builder;

  g_variant_builder_init (&actions_builder, G_VARIANT_TYPE_STRING_ARRAY);
  g_variant_builder_add (&actions_builder, "s", "Test Action");

  g_variant_builder_init (&hints_builder, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add (&hints_builder, "{sv}", "urgency", g_variant_new_byte (2));

#ifdef HAVE_GLYCIN
  if (with_pixbuf)
    {
      g_autoptr (GFile) file = NULL;
      g_autoptr (GlyLoader) loader = NULL;
      g_autoptr (GlyImage) image = NULL;
      g_autoptr (GlyFrame) frame = NULL;
      GBytes *bytes;
      GlyMemoryFormat memory_format;
      GVariant *value;
      GError *error = NULL;

      file = g_file_new_for_uri ("resource://tests/image.png");
      loader = gly_loader_new (file);
      image = gly_loader_load (loader, &error);
      g_assert_no_error (error);
      frame = gly_image_next_frame (image, &error);
      g_assert_no_error (error);

      bytes = gly_frame_get_buf_bytes (frame);
      memory_format = gly_frame_get_memory_format (frame);
      value = g_variant_new ("(iiibii@ay)",
                             gly_frame_get_width (frame),
                             gly_frame_get_height (frame),
                             gly_frame_get_stride (frame),
                             gly_memory_format_has_alpha (memory_format),
                             8, /* bits_per_sample */
                             gly_memory_format_has_alpha (memory_format) ? 4 : 3,
                             g_variant_new_from_data (G_VARIANT_TYPE ("ay"),
                                                      g_bytes_get_data (bytes, NULL),
                                                      g_bytes_get_size (bytes),
                                                      TRUE,
                                                      (GDestroyNotify)g_bytes_unref,
                                                      g_bytes_ref (bytes)));

      g_variant_builder_add (&hints_builder, "{sv}", "image-data", value);
    }
#endif /* HAVE_GLYCIN */

  notification = g_variant_new ("(susssasa{sv}i)",
                                "Test Application",
                                0, // id
                                with_pixbuf
                                  ? ""
                                  : "dialog-information-symbolic",
                                "Test Title",
                                "Test Body",
                                &actions_builder,
                                &hints_builder,
                                -1); // timeout

  g_dbus_connection_call (fixture->connection,
                          "org.freedesktop.Notifications",
                          "/org/freedesktop/Notifications",
                          "org.freedesktop.Notifications",
                          "Notify",
                          notification,
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          NULL,
                          (GAsyncReadyCallback)send_notification_cb,
                          fixture);
}

static void
test_fdo_notifications_source (FdoNotificationsFixture *fixture,
                               gconstpointer            user_data)
{
  g_autoptr (ValentNotification) notification = NULL;
  g_autoptr (GIcon) cmp_icon = NULL;
  g_autoptr (GIcon) icon = NULL;
  g_autofree char *notification_id = NULL;
  g_autofree char *id = NULL;
  g_autofree char *application = NULL;
  g_autofree char *title = NULL;
  g_autofree char *body = NULL;
  GNotificationPriority priority;

  /* Wait a bit longer for initialization to finish
   * NOTE: this is longer than most tests due to the chained async functions
   *       being called in ValentFdoNotifications.
   */
  valent_test_await_timeout (1000);
  g_signal_connect (fixture->notifications,
                    "notification-added",
                    G_CALLBACK (on_notification_added),
                    &notification);
  g_signal_connect (fixture->notifications,
                    "notification-removed",
                    G_CALLBACK (on_notification_removed),
                    &notification_id);

  VALENT_TEST_CHECK ("Adapter adds notifications");
  send_notification (fixture, FALSE);
  valent_test_await_pointer (&notification);
  g_assert_true (VALENT_IS_NOTIFICATION (notification));

  VALENT_TEST_CHECK ("Notifications have the expected content");
  cmp_icon = g_themed_icon_new ("dialog-information-symbolic");
  g_object_get (notification,
                "id",          &id,
                "application", &application,
                "title",       &title,
                "body",        &body,
                "icon",        &icon,
                "priority",    &priority,
                NULL);

  g_assert_cmpstr (application, ==, "Test Application");
  g_assert_cmpstr (title, ==, "Test Title");
  g_assert_cmpstr (body, ==, "Test Body");
  g_assert_true (g_icon_equal (icon, cmp_icon));
  g_assert_cmpuint (priority, ==, G_NOTIFICATION_PRIORITY_URGENT);
  g_clear_object (&notification);

  VALENT_TEST_CHECK ("Adapter removes notifications");
  close_notification (fixture);
  valent_test_await_pointer (&notification_id);
  g_assert_cmpstr (id, ==, notification_id);
  g_clear_pointer (&notification_id, g_free);

#ifdef HAVE_GLYCIN
  VALENT_TEST_CHECK ("Adapter adds notifications with pixbuf icons");
  send_notification (fixture, TRUE);
  valent_test_await_pointer (&notification);
  g_clear_object (&notification);
#endif /* HAVE_GLYCIN */

  g_signal_handlers_disconnect_by_data (fixture->notifications, notification);
  g_signal_handlers_disconnect_by_data (fixture->notifications, notification_id);
}

int
main (int   argc,
      char *argv[])
{
  valent_test_init (&argc, &argv, NULL);

  g_test_add ("/plugins/fdo/notifications",
              FdoNotificationsFixture, NULL,
              fdo_notifications_fixture_set_up,
              test_fdo_notifications_source,
              fdo_notifications_fixture_tear_down);

  return g_test_run ();
}
