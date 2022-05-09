// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>
#include <libvalent-core.h>
#include <libvalent-session.h>
#include <libvalent-test.h>

// See mock_session.py
#define LOGIND_SESSION_PATH "/org/freedesktop/login1/session/1"


typedef struct
{
  ValentSession   *session;
  GDBusConnection *connection;
  GMainLoop       *loop;
} FdoSessionFixture;

static void
fdo_session_fixture_set_up (FdoSessionFixture *fixture,
                            gconstpointer      user_data)
{
  g_autoptr (GSettings) settings = NULL;

  /* Disable the mock plugin */
  settings = valent_component_new_settings ("session", "mock");
  g_settings_set_boolean (settings, "enabled", FALSE);

  fixture->connection = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, NULL);
  fixture->loop = g_main_loop_new (NULL, FALSE);
  fixture->session = valent_session_get_default ();
}

static void
fdo_session_fixture_tear_down (FdoSessionFixture *fixture,
                               gconstpointer      user_data)
{
  g_clear_object (&fixture->connection);
  g_clear_pointer (&fixture->loop, g_main_loop_unref);
  v_assert_finalize_object (fixture->session);
}

static void
dbusmock_call_cb (GDBusConnection   *connection,
                  GAsyncResult      *result,
                  FdoSessionFixture *fixture)
{
  g_autoptr (GVariant) reply = NULL;
  g_autoptr (GError) error = NULL;

  reply = g_dbus_connection_call_finish (connection, result, &error);
  g_assert_no_error (error);
}

static void
dbusmock_update_property (FdoSessionFixture *fixture,
                          const char        *property_name,
                          gboolean           property_value)
{
  GVariantBuilder builder;

  g_variant_builder_init (&builder, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add (&builder, "{sv}", property_name,
                         g_variant_new_boolean (property_value));

  g_dbus_connection_call (fixture->connection,
                          "org.freedesktop.login1",
                          LOGIND_SESSION_PATH,
                          "org.freedesktop.DBus.Mock",
                          "UpdateProperties",
                          g_variant_new ("(sa{sv})",
                                         "org.freedesktop.login1.Session",
                                         &builder),
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          NULL,
                          (GAsyncReadyCallback)dbusmock_call_cb,
                          NULL);
}

static void
on_session_changed (ValentSession     *session,
                    FdoSessionFixture *fixture)
{
  g_main_loop_quit (fixture->loop);
}

static gboolean
on_timeout (gpointer data)
{
  FdoSessionFixture *fixture = data;

  g_main_loop_quit (fixture->loop);

  return G_SOURCE_REMOVE;
}

static void
test_fdo_session_adapter (FdoSessionFixture *fixture,
                          gconstpointer      user_data)
{
  /* Wait a bit longer for the D-Bus calls to resolve
   * NOTE: this is longer than most tests due to the chained async functions
   */
  g_timeout_add_seconds (1, on_timeout, fixture);
  g_main_loop_run (fixture->loop);

  g_signal_connect (fixture->session,
                    "changed",
                    G_CALLBACK (on_session_changed),
                    fixture);

  g_assert_true (valent_session_get_active (fixture->session));
  dbusmock_update_property (fixture, "Active", FALSE);
  g_main_loop_run (fixture->loop);
  g_assert_false (valent_session_get_active (fixture->session));

  g_assert_false (valent_session_get_locked (fixture->session));
  valent_session_set_locked (fixture->session, TRUE);
  g_main_loop_run (fixture->loop);
  g_assert_true (valent_session_get_locked (fixture->session));

  g_signal_handlers_disconnect_by_data (fixture->session, fixture);
}

int
main (int   argc,
      char *argv[])
{
  valent_test_init (&argc, &argv, NULL);

  g_test_add ("/plugins/fdo/session",
              FdoSessionFixture, NULL,
              fdo_session_fixture_set_up,
              test_fdo_session_adapter,
              fdo_session_fixture_tear_down);

  return g_test_run ();
}
