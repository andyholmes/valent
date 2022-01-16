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
  settings = valent_component_new_settings ("clipboard", "mock");
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
  g_autofree char *text = NULL;

  while (g_main_context_iteration (NULL, FALSE))
    continue;

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

  g_assert_cmpint (valent_clipboard_get_timestamp (fixture->clipboard), !=, 0);

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
