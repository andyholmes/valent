// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>
#include <gdk/gdk.h>
#include <valent.h>
#include <libvalent-test.h>


typedef struct
{
  ValentClipboard *clipboard;
  GMainLoop       *loop;
  gpointer         data;
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
  fixture->loop = g_main_loop_new (NULL, FALSE);
}

static void
gdk_clipboard_fixture_tear_down (GdkClipboardFixture *fixture,
                                 gconstpointer        user_data)
{
  v_assert_finalize_object (fixture->clipboard);
  g_clear_pointer (&fixture->loop, g_main_loop_unref);
  g_clear_pointer (&fixture->data, g_free);
}

static void
valent_clipboard_read_bytes_cb (ValentClipboard     *clipboard,
                                GAsyncResult        *result,
                                GdkClipboardFixture *fixture)
{
  GError *error = NULL;

  fixture->data = valent_clipboard_read_bytes_finish (clipboard, result, &error);
  g_assert_no_error (error);
  g_main_loop_quit (fixture->loop);
}

static void
valent_clipboard_write_bytes_cb (ValentClipboard     *clipboard,
                                 GAsyncResult        *result,
                                 GdkClipboardFixture *fixture)
{
  gboolean ret;
  GError *error = NULL;

  ret = valent_clipboard_write_bytes_finish (clipboard, result, &error);
  g_assert_true (ret);
  g_assert_no_error (error);
  g_main_loop_quit (fixture->loop);
}

static void
valent_clipboard_read_text_cb (ValentClipboard     *clipboard,
                               GAsyncResult        *result,
                               GdkClipboardFixture *fixture)
{
  GError *error = NULL;

  fixture->data = valent_clipboard_read_text_finish (clipboard, result, &error);
  g_assert_no_error (error);
  g_main_loop_quit (fixture->loop);
}

static void
valent_clipboard_write_text_cb (ValentClipboard     *clipboard,
                                GAsyncResult        *result,
                                GdkClipboardFixture *fixture)
{
  gboolean ret;
  GError *error = NULL;

  ret = valent_clipboard_write_text_finish (clipboard, result, &error);
  g_assert_true (ret);
  g_assert_no_error (error);
  g_main_loop_quit (fixture->loop);
}

static void
on_changed (ValentClipboard     *clipboard,
            GdkClipboardFixture *fixture)
{
  valent_clipboard_read_text (clipboard,
                             NULL,
                             (GAsyncReadyCallback)valent_clipboard_read_text_cb,
                             fixture);
}

static void
test_gdk_clipboard (GdkClipboardFixture *fixture,
                    gconstpointer        user_data)
{
  GdkDisplay *display;
  GdkClipboard *clipboard;
  g_autoptr (GBytes) bytes = NULL;
  g_autofree char *text = NULL;
  g_auto (GStrv) mimetypes = NULL;
  int64_t timestamp = 0;

  /* Data can be written */
  text = g_uuid_string_random ();
  bytes = g_bytes_new_take (text, strlen (text) + 1);
  text = NULL;
  valent_clipboard_write_bytes (fixture->clipboard,
                                "text/plain;charset=utf-8",
                                bytes,
                                NULL,
                                (GAsyncReadyCallback)valent_clipboard_write_bytes_cb,
                                fixture);
  g_main_loop_run (fixture->loop);

  /* Data can be read */
  valent_clipboard_read_bytes (fixture->clipboard,
                               "text/plain;charset=utf-8",
                               NULL,
                               (GAsyncReadyCallback)valent_clipboard_read_bytes_cb,
                               fixture);
  g_main_loop_run (fixture->loop);

  g_assert_cmpmem (g_bytes_get_data (bytes, NULL),
                   g_bytes_get_size (bytes),
                   g_bytes_get_data (fixture->data, NULL),
                   g_bytes_get_size (fixture->data));
  g_clear_pointer (&fixture->data, g_bytes_unref);

  /* Timestamp is updated */
  timestamp = valent_clipboard_get_timestamp (fixture->clipboard);
  g_assert_cmpint (timestamp, !=, 0);

  /* Mimetypes are updated */
  mimetypes = valent_clipboard_get_mimetypes (fixture->clipboard);
  g_assert_nonnull (mimetypes);
  g_assert_true (g_strv_contains ((const char * const *)mimetypes,
                                  "text/plain;charset=utf-8"));
  g_clear_pointer (&mimetypes, g_strfreev);

  /* Text can be written */
  text = g_uuid_string_random ();
  valent_clipboard_write_text (fixture->clipboard,
                               text,
                               NULL,
                               (GAsyncReadyCallback)valent_clipboard_write_text_cb,
                               fixture);
  g_main_loop_run (fixture->loop);

  /* Text can be read */
  valent_clipboard_read_text (fixture->clipboard,
                              NULL,
                              (GAsyncReadyCallback)valent_clipboard_read_text_cb,
                              fixture);
  g_main_loop_run (fixture->loop);

  g_assert_cmpstr (fixture->data, ==, text);
  g_clear_pointer (&fixture->data, g_free);
  g_clear_pointer (&text, g_free);

  /* Timestamp is updated */
  timestamp = valent_clipboard_get_timestamp (fixture->clipboard);
  g_assert_cmpint (timestamp, !=, 0);

  /* Mimetypes are updated */
  mimetypes = valent_clipboard_get_mimetypes (fixture->clipboard);
  g_assert_nonnull (mimetypes);
  g_assert_true (g_strv_contains ((const char * const *)mimetypes,
                                  "text/plain;charset=utf-8"));
  g_clear_pointer (&mimetypes, g_strfreev);

  /* Signals propagate from GdkClipboard */
  g_signal_connect (fixture->clipboard,
                    "changed",
                    G_CALLBACK (on_changed),
                    fixture);

  display = gdk_display_get_default ();
  clipboard = gdk_display_get_clipboard (display);

  text = g_uuid_string_random ();
  gdk_clipboard_set_text (clipboard, text);
  g_main_loop_run (fixture->loop);

  g_assert_cmpstr (fixture->data, ==, text);
  g_clear_pointer (&fixture->data, g_free);
  g_clear_pointer (&text, g_free);

  g_signal_handlers_disconnect_by_data (fixture->clipboard, fixture);
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
