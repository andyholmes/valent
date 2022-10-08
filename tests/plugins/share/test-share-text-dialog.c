// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <libvalent-test.h>
#include <libvalent-ui.h>

#include "valent-share-text-dialog.h"

#define TEST_TEXT "Example text with link valent.andyholmes.ca"

static void
test_share_text_dialog (void)
{
  GtkWindow *window = NULL;
  g_autofree char *text_out = NULL;

  window = g_object_new (VALENT_TYPE_SHARE_TEXT_DIALOG,
                         "text", TEST_TEXT,
                         NULL);
  g_object_add_weak_pointer (G_OBJECT (window), (gpointer)&window);

  gtk_window_present (window);

  while (g_main_context_iteration (NULL, FALSE))
    continue;

  /* Properties */
  g_object_get (window,
                "text", &text_out,
                NULL);
  g_assert_cmpstr (text_out, ==, TEST_TEXT);

  /* Destroy */
  gtk_window_destroy (window);

  while (window != NULL)
    g_main_context_iteration (NULL, FALSE);
}

static void
test_share_text_dialog_copy (void)
{
  GtkWindow *window = NULL;

  window = g_object_new (VALENT_TYPE_SHARE_TEXT_DIALOG,
                         "text", TEST_TEXT,
                         NULL);
  g_object_add_weak_pointer (G_OBJECT (window), (gpointer)&window);

  gtk_window_present (window);

  while (g_main_context_iteration (NULL, FALSE))
    continue;

  /* Copy to clipboard */
  adw_message_dialog_response (ADW_MESSAGE_DIALOG (window), "copy");

  while (window != NULL)
    g_main_context_iteration (NULL, FALSE);
}

VALENT_NO_ASAN static void
test_share_text_dialog_save (void)
{
  GtkWindow *window = NULL;

  window = g_object_new (VALENT_TYPE_SHARE_TEXT_DIALOG,
                         "text", TEST_TEXT,
                         NULL);
  g_object_add_weak_pointer (G_OBJECT (window), (gpointer)&window);

  gtk_window_present (window);

  while (g_main_context_iteration (NULL, FALSE))
    continue;

  /* Save to file */
  adw_message_dialog_response (ADW_MESSAGE_DIALOG (window), "save");

  while (g_main_context_iteration (NULL, FALSE))
    continue;

  /* The dialog gets no response, so destroy manually */
  gtk_window_destroy (window);

  while (window != NULL)
    g_main_context_iteration (NULL, FALSE);
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
  if (!valent_in_flatpak ())
    {
      g_test_add_func ("/plugins/share/text-dialog-save",
                       test_share_text_dialog_save);
    }

  return g_test_run ();
}

