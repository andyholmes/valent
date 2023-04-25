// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <valent.h>
#include <libvalent-test.h>

#include "valent-runcommand-editor.h"


static void
test_runcommand_dialog (void)
{
  g_autoptr (ValentRuncommandEditor) dialog = NULL;

  VALENT_TEST_CHECK ("Window can be constructed");
  dialog = g_object_new (VALENT_TYPE_RUNCOMMAND_EDITOR, NULL);
  gtk_window_present (GTK_WINDOW (dialog));

  VALENT_TEST_CHECK ("Window properties start empty");
  g_assert_cmpstr (valent_runcommand_editor_get_command (dialog), ==, "");
  g_assert_cmpstr (valent_runcommand_editor_get_name (dialog), ==, "");
  g_assert_cmpstr (valent_runcommand_editor_get_uuid (dialog), ==, "");

  VALENT_TEST_CHECK ("Window properties can be set");
  valent_runcommand_editor_set_command (dialog, "command");
  valent_runcommand_editor_set_name (dialog, "name");
  valent_runcommand_editor_set_uuid (dialog, "uuid");

  g_assert_cmpstr (valent_runcommand_editor_get_command (dialog), ==, "command");
  g_assert_cmpstr (valent_runcommand_editor_get_name (dialog), ==, "name");
  g_assert_cmpstr (valent_runcommand_editor_get_uuid (dialog), ==, "uuid");

  VALENT_TEST_CHECK ("Window properties can be cleared");
  valent_runcommand_editor_clear (dialog);

  g_assert_cmpstr (valent_runcommand_editor_get_command (dialog), ==, "");
  g_assert_cmpstr (valent_runcommand_editor_get_name (dialog), ==, "");
  g_assert_cmpstr (valent_runcommand_editor_get_uuid (dialog), ==, "");
}

int
main (int   argc,
      char *argv[])
{
  valent_test_ui_init (&argc, &argv, NULL);

  g_test_add_func ("/plugins/runcommand/dialog",
                   test_runcommand_dialog);

  return g_test_run ();
}

