// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <libportal/portal.h>
#include <valent.h>
#include <libvalent-test.h>

#include "valent-share-text-dialog.h"

#define TEST_TEXT "Example text with link valent.andyholmes.ca"

static void
test_share_text_dialog (void)
{
  GtkWindow *window = NULL;
  g_autofree char *text_out = NULL;

  VALENT_TEST_CHECK ("Window can be constructed");
  window = g_object_new (VALENT_TYPE_SHARE_TEXT_DIALOG,
                         "text", TEST_TEXT,
                         NULL);
  g_object_add_weak_pointer (G_OBJECT (window), (gpointer)&window);

  gtk_window_present (window);
  valent_test_await_pending ();

  VALENT_TEST_CHECK ("GObject properties function correctly");
  g_object_get (window,
                "text", &text_out,
                NULL);
  g_assert_cmpstr (text_out, ==, TEST_TEXT);

  VALENT_TEST_CHECK ("Content can be ignored");
  gtk_window_destroy (window);
  valent_test_await_nullptr (&window);
}

static void
test_share_text_dialog_copy (void)
{
  GtkWindow *window = NULL;

  VALENT_TEST_CHECK ("Window can be constructed");
  window = g_object_new (VALENT_TYPE_SHARE_TEXT_DIALOG,
                         "text", TEST_TEXT,
                         NULL);
  g_object_add_weak_pointer (G_OBJECT (window), (gpointer)&window);

  gtk_window_present (window);
  valent_test_await_pending ();

  VALENT_TEST_CHECK ("Content can be copied to the clipboard");
  adw_message_dialog_response (ADW_MESSAGE_DIALOG (window), "copy");
  valent_test_await_nullptr (&window);
}

static void
test_share_text_dialog_save (void)
{
  GtkWindow *window = NULL;

  VALENT_TEST_CHECK ("Window can be constructed");
  window = g_object_new (VALENT_TYPE_SHARE_TEXT_DIALOG,
                         "text", TEST_TEXT,
                         NULL);
  g_object_add_weak_pointer (G_OBJECT (window), (gpointer)&window);

  gtk_window_present (window);
  valent_test_await_pending ();

#if !(VALENT_HAVE_ASAN)
  VALENT_TEST_CHECK ("Content can be saved to file");
  adw_message_dialog_response (ADW_MESSAGE_DIALOG (window), "save");
  valent_test_await_pending ();
#endif // !(VALENT_HAVE_ASAN)

  /* The dialog gets no response, so destroy manually */
  gtk_window_destroy (window);
  valent_test_await_nullptr (&window);
}

int
main (int   argc,
      char *argv[])
{
  valent_test_ui_init (&argc, &argv, NULL);

  g_test_add_func ("/plugins/share/text-dialog",
                   test_share_text_dialog);
  g_test_add_func ("/plugins/share/text-dialog-copy",
                   test_share_text_dialog_copy);

  // FIXME: Settings schema 'org.gtk.gtk4.Settings.FileChooser' is not installed
  if (!xdp_portal_running_under_flatpak ())
    {
      g_test_add_func ("/plugins/share/text-dialog-save",
                       test_share_text_dialog_save);
    }

  return g_test_run ();
}

