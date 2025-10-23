// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include "config.h"

#include <gio/gio.h>
#include <valent.h>
#include <libvalent-test.h>

typedef struct
{
  GDBusConnection     *connection;
} FdoNotificationsFixture;

static void
fdo_notifications_fixture_set_up (FdoNotificationsFixture *fixture,
                                  gconstpointer            user_data)
{
  fixture->connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
}

static void
fdo_notifications_fixture_tear_down (FdoNotificationsFixture *fixture,
                                     gconstpointer            user_data)
{
  g_clear_object (&fixture->connection);
}

static void
close_notification_cb (GDBusConnection         *connection,
                       GAsyncResult            *result,
                       FdoNotificationsFixture *fixture)
{
  g_autoptr (GVariant) reply = NULL;
  GError *error = NULL;

  reply = g_dbus_connection_call_finish (connection, result, &error);
  g_assert_no_error (error);
}

static void
close_notification (FdoNotificationsFixture *fixture,
                    GVariant                *notification_id)
{
  g_dbus_connection_call (fixture->connection,
                          "org.freedesktop.Notifications",
                          "/org/freedesktop/Notifications",
                          "org.freedesktop.Notifications",
                          "CloseNotification",
                          notification_id,
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          NULL,
                          (GAsyncReadyCallback)close_notification_cb,
                          fixture);
}

static void
send_notification_cb (GDBusConnection  *connection,
                      GAsyncResult     *result,
                      GVariant        **id_out)
{
  GError *error = NULL;

  *id_out = g_dbus_connection_call_finish (connection, result, &error);
  g_assert_no_error (error);
}

static void
send_notification (FdoNotificationsFixture  *fixture,
                   gboolean                  with_pixbuf,
                   GVariant                **id_out)
{
  GVariantBuilder actions_builder;
  GVariantBuilder hints_builder;

  g_variant_builder_init (&actions_builder, G_VARIANT_TYPE_STRING_ARRAY);
  g_variant_builder_add (&actions_builder, "s", "Test Action");

  g_variant_builder_init (&hints_builder, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add (&hints_builder, "{sv}", "urgency", g_variant_new_byte (2));

  if (with_pixbuf)
    {
      GVariant *value;

      value = g_variant_new ("(iiibii@ay)",
                             1,     // width
                             1,     // height
                             3,     // stride
                             FALSE, // has_alpha
                             8,     // bits_per_sample
                             3,     // channels
                             g_variant_new_from_data (G_VARIANT_TYPE ("ay"),
                                                      "\x00\x00\x00", 3,
                                                      TRUE,
                                                      NULL,
                                                      NULL));
      g_variant_builder_add (&hints_builder, "{sv}", "image-data", value);
    }

  g_dbus_connection_call (fixture->connection,
                          "org.freedesktop.Notifications",
                          "/org/freedesktop/Notifications",
                          "org.freedesktop.Notifications",
                          "Notify",
                          g_variant_new ("(susssasa{sv}i)",
                                         "Test Application",
                                         0, // replaces_id
                                         with_pixbuf
                                           ? ""
                                           : "dialog-information-symbolic",
                                         "Test Title",
                                         "Test Body",
                                         &actions_builder,
                                         &hints_builder,
                                         -1), // timeout,
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          NULL,
                          (GAsyncReadyCallback)send_notification_cb,
                          id_out);
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
test_fdo_notifications_source (FdoNotificationsFixture *fixture,
                               gconstpointer            user_data)
{
  PeasEngine *engine;
  PeasPluginInfo *plugin_info;
  g_autoptr (ValentContext) context = NULL;
  g_autoptr (GObject) adapter = NULL;
  g_autoptr (ValentNotification) notification = NULL;
  g_autoptr (GIcon) cmp_icon = NULL;
  g_autoptr (GIcon) icon = NULL;
  g_autoptr (GVariant) id_value = NULL;
  uint32_t id_out;
  g_autofree char *id_str = NULL;
  g_autofree char *id = NULL;
  g_autofree char *application = NULL;
  g_autofree char *title = NULL;
  g_autofree char *body = NULL;
  GNotificationPriority priority;
  gboolean done = FALSE;

  engine = valent_get_plugin_engine ();
  plugin_info = peas_engine_get_plugin_info (engine, "fdo");
  context = valent_context_new (NULL, "plugin", "fdo");

  VALENT_TEST_CHECK ("Adapter can be constructed");
  adapter = peas_engine_create_extension (engine,
                                          plugin_info,
                                          VALENT_TYPE_NOTIFICATIONS_ADAPTER,
                                          "iri",     "urn:valent:notifications:fdo",
                                          "source",  NULL,
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
  send_notification (fixture, FALSE, &id_value);
  valent_test_await_pointer (&id_value);
  valent_test_await_pointer (&notification);

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

  g_variant_get (id_value, "(u)", &id_out);
  id_str = g_strdup_printf ("%u", id_out);

  g_assert_cmpstr (id, ==, id_str);
  g_assert_cmpstr (application, ==, "Test Application");
  g_assert_cmpstr (title, ==, "Test Title");
  g_assert_cmpstr (body, ==, "Test Body");
  g_assert_true (g_icon_equal (icon, cmp_icon));
  g_assert_cmpuint (priority, ==, G_NOTIFICATION_PRIORITY_URGENT);

  VALENT_TEST_CHECK ("Adapter removes notifications");
  close_notification (fixture, id_value);
  valent_test_await_nullptr (&notification);

  g_clear_pointer (&id, g_free);
  g_clear_pointer (&application, g_free);
  g_clear_pointer (&title, g_free);
  g_clear_pointer (&body, g_free);
  g_clear_object (&icon);
  g_clear_pointer (&id_value, g_variant_unref);
  g_clear_pointer (&id_str, g_free);

#ifdef HAVE_GLYCIN
  VALENT_TEST_CHECK ("Adapter adds notifications with pixbuf icons");
  send_notification (fixture, TRUE, &id_value);
  valent_test_await_pointer (&id_value);
  valent_test_await_pointer (&notification);

  VALENT_TEST_CHECK ("Notifications with pixbuf icons have the expected content");
  g_object_get (notification,
                "id",          &id,
                "application", &application,
                "title",       &title,
                "body",        &body,
                "icon",        &icon,
                "priority",    &priority,
                NULL);

  g_variant_get (id_value, "(u)", &id_out);
  id_str = g_strdup_printf ("%u", id_out);

  g_assert_cmpstr (id, ==, id_str);
  g_assert_cmpstr (application, ==, "Test Application");
  g_assert_cmpstr (title, ==, "Test Title");
  g_assert_cmpstr (body, ==, "Test Body");
  g_assert_true (G_IS_ICON (icon));
  g_assert_cmpuint (priority, ==, G_NOTIFICATION_PRIORITY_URGENT);

  VALENT_TEST_CHECK ("Adapter removes notifications with pixbuf icons");
  close_notification (fixture, id_value);
  valent_test_await_nullptr (&notification);

  g_clear_pointer (&id, g_free);
  g_clear_pointer (&application, g_free);
  g_clear_pointer (&title, g_free);
  g_clear_pointer (&body, g_free);
  g_clear_object (&icon);
  g_clear_pointer (&id_value, g_variant_unref);
  g_clear_pointer (&id_str, g_free);
#endif /* HAVE_GLYCIN */

  g_signal_handlers_disconnect_by_data (adapter, &notification);
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
