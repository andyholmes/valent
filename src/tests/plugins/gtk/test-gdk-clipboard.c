// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>
#include <libvalent-core.h>
#include <libvalent-clipboard.h>
#include <libvalent-test.h>


typedef struct
{
  ValentClipboard *clipboard;
  GMainLoop       *loop;
  gpointer         data;
} GdkClipboardFixture;

static void
clipboard_component_fixture_set_up (GdkClipboardFixture *fixture,
                                    gconstpointer        user_data)
{
  g_autoptr (GSettings) settings = NULL;

  /* Disable the mock plugin */
  settings = valent_component_create_settings ("clipboard", "mock");
  g_settings_set_boolean (settings, "enabled", FALSE);

  fixture->clipboard = valent_clipboard_get_default ();
  fixture->loop = g_main_loop_new (NULL, FALSE);
}

static void
clipboard_component_fixture_tear_down (GdkClipboardFixture *fixture,
                                       gconstpointer        user_data)
{
  v_assert_finalize_object (fixture->clipboard);
  g_clear_pointer (&fixture->loop, g_main_loop_unref);
  g_clear_pointer (&fixture->data, g_free);
}

static void
valent_clipboard_get_bytes_cb (ValentClipboard     *clipboard,
                               GAsyncResult        *result,
                               GdkClipboardFixture *fixture)
{
  GError *error = NULL;

  fixture->data = valent_clipboard_get_bytes_finish (clipboard, result, &error);
  g_assert_no_error (error);
  g_main_loop_quit (fixture->loop);
}

static void
get_text_cb (ValentClipboard     *clipboard,
             GAsyncResult        *result,
             GdkClipboardFixture *fixture)
{
  GError *error = NULL;

  fixture->data = valent_clipboard_get_text_finish (clipboard, result, &error);
  g_assert_no_error (error);
  g_main_loop_quit (fixture->loop);
}

static void
on_changed (ValentClipboard     *clipboard,
            GdkClipboardFixture *fixture)
{
  valent_clipboard_get_text_async (clipboard,
                                   NULL,
                                   (GAsyncReadyCallback)get_text_cb,
                                   fixture);
}

static void
test_gdk_clipboard (GdkClipboardFixture *fixture,
                    gconstpointer              user_data)
{
  GdkDisplay *display;
  GdkClipboard *clipboard;
  g_autoptr (GBytes) bytes = NULL;
  g_autofree char *text = NULL;
  g_auto (GStrv) mimetypes = NULL;
  gint64 timestamp = 0;

  while (g_main_context_iteration (NULL, FALSE))
    continue;

  /* Clipboard data can be written */
  text = g_uuid_string_random ();
  bytes = g_bytes_new_take (text, strlen (text) + 1);
  text = NULL;
  valent_clipboard_set_bytes (fixture->clipboard,
                              "text/plain;charset=utf-8",
                              bytes);

  /* Clipboard data can be read */
  valent_clipboard_get_bytes (fixture->clipboard,
                              "text/plain;charset=utf-8",
                              NULL,
                              (GAsyncReadyCallback)valent_clipboard_get_bytes_cb,
                              fixture);
  g_main_loop_run (fixture->loop);

  g_assert_cmpmem (g_bytes_get_data (bytes, NULL),
                   g_bytes_get_size (bytes),
                   g_bytes_get_data (fixture->data, NULL),
                   g_bytes_get_size (fixture->data));
  g_clear_pointer (&fixture->data, g_bytes_unref);

  /* Clipboard timestamp is updated */
  timestamp = valent_clipboard_get_timestamp (fixture->clipboard);
  g_assert_cmpint (timestamp, !=, 0);

  /* Clipboard mimetypes are updated */
  mimetypes = valent_clipboard_get_mimetypes (fixture->clipboard);
  g_assert_nonnull (mimetypes);
  g_assert_true (g_strv_contains ((const char * const *)mimetypes,
                                  "text/plain;charset=utf-8"));
  g_clear_pointer (&mimetypes, g_strfreev);

  /* Clipboard text can be written */
  text = g_uuid_string_random ();
  valent_clipboard_set_text (fixture->clipboard, text);

  /* Clipboard text can be read */
  valent_clipboard_get_text_async (fixture->clipboard,
                                   NULL,
                                   (GAsyncReadyCallback)get_text_cb,
                                   fixture);
  g_main_loop_run (fixture->loop);

  g_assert_cmpstr (fixture->data, ==, text);
  g_clear_pointer (&fixture->data, g_free);
  g_clear_pointer (&text, g_free);

  /* Clipboard timestamp is updated */
  timestamp = valent_clipboard_get_timestamp (fixture->clipboard);
  g_assert_cmpint (timestamp, !=, 0);

  /* Clipboard mimetypes are updated */
  mimetypes = valent_clipboard_get_mimetypes (fixture->clipboard);
  g_assert_nonnull (mimetypes);
  g_assert_true (g_strv_contains ((const char * const *)mimetypes,
                                  "text/plain;charset=utf-8"));
  g_clear_pointer (&mimetypes, g_strfreev);

  /* Signals fire */
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
              clipboard_component_fixture_set_up,
              test_gdk_clipboard,
              clipboard_component_fixture_tear_down);

  return g_test_run ();
}
