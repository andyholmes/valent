// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>
#include <gdk/gdk.h>
#include <valent.h>
#include <libvalent-test.h>


typedef struct
{
  ValentClipboard *clipboard;
} GdkClipboardFixture;

static void
gdk_clipboard_fixture_set_up (GdkClipboardFixture *fixture,
                              gconstpointer        user_data)
{
  g_autoptr (GSettings) settings = NULL;

  /* Disable the mock plugin */
  settings = valent_test_mock_settings ("clipboard");
  g_settings_set_boolean (settings, "enabled", FALSE);

  fixture->clipboard = valent_clipboard_get_default ();
}

static void
gdk_clipboard_fixture_tear_down (GdkClipboardFixture *fixture,
                                 gconstpointer        user_data)
{
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
valent_clipboard_read_text_cb (ValentClipboard  *clipboard,
                               GAsyncResult     *result,
                               char            **text)
{
  GError *error = NULL;

  if (text != NULL)
    *text = valent_clipboard_read_text_finish (clipboard, result, &error);

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
on_changed (ValentClipboard  *clipboard,
            char            **text)
{
  valent_clipboard_read_text (clipboard,
                             NULL,
                             (GAsyncReadyCallback)valent_clipboard_read_text_cb,
                             text);
}

static void
test_gdk_clipboard (GdkClipboardFixture *fixture,
                    gconstpointer        user_data)
{
  GdkDisplay *display;
  GdkClipboard *clipboard;
  g_autoptr (GBytes) bytes = NULL;
  g_autoptr (GBytes) bytes_read = NULL;
  g_autofree char *text = NULL;
  g_autofree char *text_read = NULL;
  g_auto (GStrv) mimetypes = NULL;
  int64_t timestamp = 0;
  gboolean done = FALSE;

  VALENT_TEST_CHECK ("Adapter handles bytes written to the clipboard");
  text = g_uuid_string_random ();
  bytes = g_bytes_new_take (text, strlen (text) + 1);
  text = NULL;
  valent_clipboard_write_bytes (fixture->clipboard,
                                "text/plain;charset=utf-8",
                                bytes,
                                NULL,
                                (GAsyncReadyCallback)valent_clipboard_write_bytes_cb,
                                &done);
  valent_test_await_boolean (&done);

  VALENT_TEST_CHECK ("Adapter handles bytes read from the clipboard");
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
  g_clear_pointer (&bytes_read, g_bytes_unref);

  VALENT_TEST_CHECK ("Adapter updates the content timestamp");
  timestamp = valent_clipboard_get_timestamp (fixture->clipboard);
  g_assert_cmpint (timestamp, !=, 0);

  VALENT_TEST_CHECK ("Adapter updates the content mime-types");
  mimetypes = valent_clipboard_get_mimetypes (fixture->clipboard);
  g_assert_nonnull (mimetypes);
  g_assert_true (g_strv_contains ((const char * const *)mimetypes,
                                  "text/plain;charset=utf-8"));
  g_clear_pointer (&mimetypes, g_strfreev);

  VALENT_TEST_CHECK ("Adapter handles text written to the clipboard");
  text = g_uuid_string_random ();
  valent_clipboard_write_text (fixture->clipboard,
                               text,
                               NULL,
                               (GAsyncReadyCallback)valent_clipboard_write_text_cb,
                               &done);
  valent_test_await_boolean (&done);

  VALENT_TEST_CHECK ("Adapter handles text read from the clipboard");
  valent_clipboard_read_text (fixture->clipboard,
                              NULL,
                              (GAsyncReadyCallback)valent_clipboard_read_text_cb,
                              &text_read);
  valent_test_await_pointer (&text_read);

  g_assert_cmpstr (text_read, ==, text);
  g_clear_pointer (&text_read, g_free);
  g_clear_pointer (&text, g_free);

  VALENT_TEST_CHECK ("Adapter updates the content timestamp");
  timestamp = valent_clipboard_get_timestamp (fixture->clipboard);
  g_assert_cmpint (timestamp, !=, 0);

  VALENT_TEST_CHECK ("Adapter updates the content mime-types");
  mimetypes = valent_clipboard_get_mimetypes (fixture->clipboard);
  g_assert_nonnull (mimetypes);
  g_assert_true (g_strv_contains ((const char * const *)mimetypes,
                                  "text/plain;charset=utf-8"));
  g_clear_pointer (&mimetypes, g_strfreev);

  VALENT_TEST_CHECK ("Adapter emits `ValentClipboardAdapter::changed`");
  g_signal_connect (fixture->clipboard,
                    "changed",
                    G_CALLBACK (on_changed),
                    &text_read);
  display = gdk_display_get_default ();
  clipboard = gdk_display_get_clipboard (display);

  text = g_uuid_string_random ();
  gdk_clipboard_set_text (clipboard, text);
  valent_test_await_pointer (&text_read);

  g_assert_cmpstr (text_read, ==, text);
  g_clear_pointer (&text_read, g_free);
  g_clear_pointer (&text, g_free);

  g_signal_handlers_disconnect_by_data (fixture->clipboard, &text_read);
}

int
main (int   argc,
      char *argv[])
{
  valent_test_ui_init (&argc, &argv, G_TEST_OPTION_ISOLATE_DIRS, NULL);

  g_test_add ("/plugins/gtk/clipboard",
              GdkClipboardFixture, NULL,
              gdk_clipboard_fixture_set_up,
              test_gdk_clipboard,
              gdk_clipboard_fixture_tear_down);

  return g_test_run ();
}
