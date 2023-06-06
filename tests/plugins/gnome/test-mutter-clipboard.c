// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <locale.h>

#include <gio/gio.h>
#include <gtk/gtk.h>
#include <valent.h>
#include <libvalent-test.h>

#define CLIPBOARD_NAME "org.gnome.Mutter.RemoteDesktop"
#define CLIPBOARD_PATH "/org/gnome/Mutter/RemoteDesktop"
#define CLIPBOARD_IFACE "org.gnome.Mutter.RemoteDesktop"


typedef struct
{
  ValentClipboard *clipboard;
  GDBusConnection *connection;
} MutterClipboardFixture;

static void
mutter_clipboard_fixture_set_up (MutterClipboardFixture *fixture,
                                 gconstpointer           user_data)
{
  g_autoptr (GSettings) settings = NULL;

  /* Disable the mock plugin */
  settings = valent_test_mock_settings ("clipboard");
  g_settings_set_boolean (settings, "enabled", FALSE);

  fixture->clipboard = valent_clipboard_get_default ();
  fixture->connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
}

static void
mutter_clipboard_fixture_tear_down (MutterClipboardFixture *fixture,
                                    gconstpointer           user_data)
{
  g_clear_object (&fixture->connection);
  v_assert_finalize_object (fixture->clipboard);
}

static void
valent_clipboard_read_bytes_cb (ValentClipboard  *clipboard,
                                GAsyncResult     *result,
                                GBytes          **bytes)
{
  GError *error = NULL;

  if (bytes != NULL)
    *bytes = valent_clipboard_read_bytes_finish (clipboard, result, &error);

  g_assert_no_error (error);
}

static void
valent_clipboard_write_bytes_cb (ValentClipboard *clipboard,
                                 GAsyncResult    *result,
                                 gboolean        *done)
{
  GError *error = NULL;

  if (done != NULL)
    *done = valent_clipboard_write_bytes_finish (clipboard, result, &error);

  g_assert_no_error (error);
}

static void
valent_clipboard_read_text_cb (ValentClipboard *clipboard,
                               GAsyncResult    *result,
                               gpointer        *data)
{
  GError *error = NULL;

  if (data != NULL)
    *data = valent_clipboard_read_text_finish (clipboard, result, &error);

  g_assert_no_error (error);
}

static void
valent_clipboard_write_text_cb (ValentClipboard *clipboard,
                                GAsyncResult    *result,
                                gboolean        *done)
{
  GError *error = NULL;

  if (done != NULL)
    *done = valent_clipboard_write_text_finish (clipboard, result, &error);

  g_assert_no_error (error);
}

static void
get_bytes_cb (GDBusConnection  *connection,
              GAsyncResult     *result,
              char            **text)
{
  g_autoptr (GVariant) reply = NULL;
  g_autoptr (GVariant) content = NULL;
  const char *data = NULL;
  size_t data_len = 0;
  g_autoptr (GError) error = NULL;

  reply = g_dbus_connection_call_finish (connection, result, &error);
  g_assert_no_error (error);

  content = g_variant_get_child_value (reply, 0);
  data = g_variant_get_fixed_array (content, &data_len, sizeof (char));
  g_assert_true (g_utf8_validate_len (data, data_len, NULL));

  if (text != NULL)
    *text = g_strndup (data, data_len);
}

static inline void
get_bytes (MutterClipboardFixture  *fixture,
           char                   **text)
{
  g_dbus_connection_call (fixture->connection,
                          CLIPBOARD_NAME,
                          CLIPBOARD_PATH,
                          CLIPBOARD_IFACE,
                          "GetBytes",
                          g_variant_new ("(s)", "text/plain;charset=utf-8"),
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          NULL,
                          (GAsyncReadyCallback)get_bytes_cb,
                          text);
}

static void
set_bytes_cb (GDBusConnection        *connection,
              GAsyncResult           *result,
              MutterClipboardFixture *fixture)
{
  g_autoptr (GVariant) reply = NULL;
  g_autoptr (GError) error = NULL;

  reply = g_dbus_connection_call_finish (connection, result, &error);
  g_assert_no_error (error);
}

static inline void
set_bytes (MutterClipboardFixture *fixture,
           const char             *text)
{
  GVariant *content = NULL;

  content = g_variant_new_fixed_array (G_VARIANT_TYPE_BYTE,
                                       text,
                                       strlen (text),
                                       sizeof (char));
  g_dbus_connection_call (fixture->connection,
                          CLIPBOARD_NAME,
                          CLIPBOARD_PATH,
                          CLIPBOARD_IFACE,
                          "SetBytes",
                          g_variant_new ("(s@ay)", "text/plain;charset=utf-8",
                                         content),
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          NULL,
                          (GAsyncReadyCallback)set_bytes_cb,
                          NULL);
}

static void
test_mutter_clipboard_adapter (MutterClipboardFixture *fixture,
                               gconstpointer           user_data)
{
  g_autoptr (GBytes) bytes = NULL;
  g_autoptr (GBytes) bytes_read = NULL;
  g_autofree char *text = NULL;
  g_autofree char *text_read = NULL;
  g_auto (GStrv) mimetypes = NULL;
  int64_t timestamp = 0;

  /* Wait a bit longer for initialization to finish
   * NOTE: this is longer than most tests due to the chained async functions
   *       being called in ValentMutterClipboard.
   */
  valent_test_await_timeout (1000);

  VALENT_TEST_CHECK ("Adapter handles data written to the clipboard");
  text = g_uuid_string_random ();
  bytes = g_bytes_new (text, strlen (text) + 1);
  valent_clipboard_write_bytes (fixture->clipboard,
                                "text/plain;charset=utf-8",
                                bytes,
                                NULL,
                                (GAsyncReadyCallback)valent_clipboard_write_bytes_cb,
                                NULL);
  valent_test_await_signal (fixture->clipboard, "changed");

  VALENT_TEST_CHECK ("Adapter handles data read from the clipboard");
  valent_clipboard_read_bytes (fixture->clipboard,
                               "text/plain;charset=utf-8",
                               NULL,
                               (GAsyncReadyCallback)valent_clipboard_read_bytes_cb,
                               &bytes_read);
  valent_test_await_pointer (&bytes_read);

  g_assert_cmpmem (g_bytes_get_data (bytes, NULL),
                   g_bytes_get_size (bytes),
                   g_bytes_get_data (bytes_read, NULL),
                   g_bytes_get_size (bytes_read));
  g_clear_pointer (&bytes, g_bytes_unref);
  g_clear_pointer (&bytes_read, g_bytes_unref);
  g_clear_pointer (&text, g_free);

  VALENT_TEST_CHECK ("Adapter handles text written to the clipboard");
  text = g_uuid_string_random ();
  valent_clipboard_write_text (fixture->clipboard,
                               text,
                               NULL,
                               (GAsyncReadyCallback)valent_clipboard_write_text_cb,
                               NULL);
  valent_test_await_signal (fixture->clipboard, "changed");

  VALENT_TEST_CHECK ("Adapter handles text read from the clipboard");
  valent_clipboard_read_text (fixture->clipboard,
                              NULL,
                              (GAsyncReadyCallback)valent_clipboard_read_text_cb,
                              &text_read);
  valent_test_await_pointer (&text_read);

  g_assert_cmpstr (text, ==, text_read);
  g_clear_pointer (&text, g_free);
  g_clear_pointer (&text_read, g_free);


  VALENT_TEST_CHECK ("Adapter propagates data written to the clipboard");
  text = g_uuid_string_random ();
  set_bytes (fixture, text);
  valent_test_await_signal (fixture->clipboard, "changed");

  VALENT_TEST_CHECK ("Adapter propagates the clipboard timestamp");
  timestamp = valent_clipboard_get_timestamp (fixture->clipboard);
  g_assert_cmpint (timestamp, !=, 0);

  VALENT_TEST_CHECK ("Adapter propagates the clipboard mimetypes");
  mimetypes = valent_clipboard_get_mimetypes (fixture->clipboard);
  g_assert_nonnull (mimetypes);
  g_assert_true (g_strv_contains ((const char * const *)mimetypes,
                                  "text/plain;charset=utf-8"));
  g_clear_pointer (&mimetypes, g_strfreev);

  VALENT_TEST_CHECK ("Adapter propagates data read from the clipboard");
  get_bytes (fixture, &text_read);
  valent_test_await_pointer (&text_read);

  g_assert_cmpstr (text, ==, text_read);
  g_clear_pointer (&text, g_free);
  g_clear_pointer (&text_read, g_free);
}

int
main (int   argc,
      char *argv[])
{
  valent_test_init (&argc, &argv, NULL);

  g_test_add ("/plugins/gnome/clipboard",
              MutterClipboardFixture, NULL,
              mutter_clipboard_fixture_set_up,
              test_mutter_clipboard_adapter,
              mutter_clipboard_fixture_tear_down);

  return g_test_run ();
}
