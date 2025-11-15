// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <locale.h>

#include <gio/gio.h>
#include <gtk/gtk.h>
#include <valent.h>
#include <libvalent-test.h>


typedef struct
{
  GDBusConnection     *connection;
} GtkNotificationsFixture;

static void
gtk_notifications_fixture_set_up (GtkNotificationsFixture *fixture,
                                  gconstpointer            user_data)
{
  fixture->connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
}

static void
gtk_notifications_fixture_tear_down (GtkNotificationsFixture *fixture,
                                     gconstpointer            user_data)
{
  g_clear_object (&fixture->connection);
}

static void
notification_cb (GDBusConnection         *connection,
                 GAsyncResult            *result,
                 GtkNotificationsFixture *fixture)
{
  g_autoptr (GVariant) reply = NULL;
  GError *error = NULL;

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
g_async_initable_init_async_cb (GAsyncInitable *initable,
                                GAsyncResult   *result,
                                gboolean       *done)
{
  GError *error = NULL;

  *done = g_async_initable_init_finish (initable, result, &error);
  g_assert_no_error (error);
  g_assert_true (*done);
}

static void
test_gtk_notifications_source (GtkNotificationsFixture *fixture,
                               gconstpointer            user_data)
{
  PeasEngine *engine;
  PeasPluginInfo *plugin_info;
  g_autoptr (ValentContext) context = NULL;
  g_autoptr (GObject) adapter = NULL;
  g_autoptr (ValentNotification) notification = NULL;
  g_autoptr (GIcon) cmp_icon = NULL;
  g_autoptr (GIcon) icon = NULL;
  g_autofree char *id = NULL;
  g_autofree char *application = NULL;
  g_autofree char *title = NULL;
  g_autofree char *body = NULL;
  GNotificationPriority priority;
  gboolean done = FALSE;

  engine = valent_get_plugin_engine ();
  plugin_info = peas_engine_get_plugin_info (engine, "gtk");
  context = valent_context_new (NULL, "plugin", "gtk");

  VALENT_TEST_CHECK ("Adapter can be constructed");
  adapter = peas_engine_create_extension (engine,
                                          plugin_info,
                                          VALENT_TYPE_NOTIFICATIONS_ADAPTER,
                                          "iri",     "urn:valent:notifications:gtk",
                                          "parent",  NULL,
                                          "context", context,
                                          NULL);
  g_async_initable_init_async (G_ASYNC_INITABLE (adapter),
                               G_PRIORITY_DEFAULT,
                               NULL,
                               (GAsyncReadyCallback)g_async_initable_init_async_cb,
                               &done);
  valent_test_await_boolean (&done);
  valent_test_await_signal (adapter, "notify::plugin-state");

  g_signal_connect (adapter,
                    "items-changed",
                    G_CALLBACK (on_items_changed),
                    &notification);

  VALENT_TEST_CHECK ("Adapter adds notifications");
  add_notification (fixture);
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

  /* g_assert_cmpstr (application, ==, "Test Application"); */
  g_assert_cmpstr (title, ==, "Test Title");
  g_assert_cmpstr (body, ==, "Test Body");
  g_assert_true (g_icon_equal (icon, cmp_icon));
  g_assert_cmpuint (priority, ==, G_NOTIFICATION_PRIORITY_URGENT);

  VALENT_TEST_CHECK ("Adapter removes notifications");
  remove_notification (fixture);
  valent_test_await_nullptr (&notification);

  g_signal_handlers_disconnect_by_data (adapter, &notification);
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
