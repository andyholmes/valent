// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gdk/gdk.h>
#include <gio/gio.h>
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
} FdoNotificationsFixture;

static void
on_notification_added (ValentNotifications     *notifications,
                       ValentNotification      *notification,
                       FdoNotificationsFixture *fixture)
{
  g_set_object (&fixture->notification, notification);
  g_main_loop_quit (fixture->loop);
}

static void
on_notification_removed (ValentNotifications     *notifications,
                         const char              *id,
                         FdoNotificationsFixture *fixture)
{
  g_clear_pointer (&fixture->notification_id, g_free);
  fixture->notification_id = g_strdup (id);
  g_main_loop_quit (fixture->loop);
}

static void
fdo_notifications_fixture_set_up (FdoNotificationsFixture *fixture,
                                  gconstpointer            user_data)
{
  g_autoptr (GSettings) settings = NULL;

  /* Disable the mock plugin */
  settings = valent_component_new_settings ("notifications", "mock");
  g_settings_set_boolean (settings, "enabled", FALSE);

  fixture->connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
  fixture->loop = g_main_loop_new (NULL, FALSE);
  fixture->notifications = valent_notifications_get_default ();
}

static void
fdo_notifications_fixture_tear_down (FdoNotificationsFixture *fixture,
                                     gconstpointer            user_data)
{
  g_clear_object (&fixture->connection);
  g_clear_pointer (&fixture->loop, g_main_loop_unref);
  g_clear_pointer (&fixture->notification_id, g_free);
  v_assert_finalize_object (fixture->notification);
  v_assert_finalize_object (fixture->notifications);
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

  if (with_pixbuf)
    {
      g_autoptr (GdkPixbuf) pixbuf = NULL;
      int width, height, rowstride, bits_per_sample, n_channels;
      guchar *pixels;
      gsize pixels_len;
      gboolean has_alpha;
      GVariant *value;

      // TODO: fix pixbuf loader problem
      pixbuf = gdk_pixbuf_new_from_file (TEST_DATA_DIR"image.png", NULL);
      g_object_get (pixbuf,
                    "width",           &width,
                    "height",          &height,
                    "rowstride",       &rowstride,
                    "n-channels",      &n_channels,
                    "bits-per-sample", &bits_per_sample,
                    "pixels",          &pixels,
                    "has-alpha",       &has_alpha,
                    NULL);
      pixels_len = (height - 1) * rowstride + width *
        ((n_channels * bits_per_sample + 7) / 8);

      value = g_variant_new ("(iiibii@ay)",
                             width,
                             height,
                             rowstride,
                             has_alpha,
                             bits_per_sample,
                             n_channels,
                             g_variant_new_from_data (G_VARIANT_TYPE ("ay"),
                                                      pixels,
                                                      pixels_len,
                                                      TRUE,
                                                      (GDestroyNotify)g_object_unref,
                                                      g_object_ref (pixbuf)));

      g_variant_builder_add (&hints_builder, "{sv}", "image-data", value);
    }

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

static gboolean
on_timeout (gpointer data)
{
  FdoNotificationsFixture *fixture = data;

  g_main_loop_quit (fixture->loop);

  return G_SOURCE_REMOVE;
}

static void
test_fdo_notifications_source (FdoNotificationsFixture *fixture,
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
   *       being called in ValentFdoNotifications.
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
  send_notification (fixture, FALSE);
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

  g_assert_cmpstr (application, ==, "Test Application");
  g_assert_cmpstr (title, ==, "Test Title");
  g_assert_cmpstr (body, ==, "Test Body");
  g_assert_true (g_icon_equal (icon, cmp_icon));
  g_assert_cmpuint (priority, ==, G_NOTIFICATION_PRIORITY_URGENT);

  /* Remove Notification */
  close_notification (fixture);
  g_main_loop_run (fixture->loop);
  g_assert_cmpstr (id, ==, fixture->notification_id);

  /* TODO: Add notification (with pixbuf) */
  // send_notification (fixture, TRUE);
  // g_main_loop_run (fixture->loop);

  g_signal_handlers_disconnect_by_data (fixture->notifications, fixture);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, G_TEST_OPTION_ISOLATE_DIRS, NULL);

  g_test_add ("/plugins/fdo/notifications",
              FdoNotificationsFixture, NULL,
              fdo_notifications_fixture_set_up,
              test_fdo_notifications_source,
              fdo_notifications_fixture_tear_down);

  return g_test_run ();
}
