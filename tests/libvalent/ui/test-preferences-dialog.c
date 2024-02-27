// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <valent.h>
#include <libvalent-test.h>

#define VALENT_TYPE_TEST_SUBJECT (g_type_from_name ("ValentPreferencesDialog"))


static void
test_preferences_dialog_basic (void)
{
  AdwDialog *dialog;
  PeasEngine *engine;
  PeasPluginInfo *info;

  dialog = g_object_new (VALENT_TYPE_TEST_SUBJECT,
                        NULL);
  g_object_add_weak_pointer (G_OBJECT (dialog), (gpointer)&dialog);

  adw_dialog_present (dialog, NULL);
  valent_test_await_pending ();

  /* Unload/Load the plugin */
  engine = valent_get_plugin_engine ();
  info = peas_engine_get_plugin_info (engine, "mock");
  peas_engine_unload_plugin (engine, info);
  peas_engine_load_plugin (engine, info);

  adw_dialog_force_close (dialog);
  valent_test_await_nullptr (&dialog);
}

static void
test_preferences_dialog_navigation (void)
{
  AdwDialog *dialog;

  dialog = g_object_new (VALENT_TYPE_TEST_SUBJECT,
                        NULL);
  g_object_add_weak_pointer (G_OBJECT (dialog), (gpointer)&dialog);

  adw_dialog_present (dialog, NULL);
  valent_test_await_pending ();

  /* Main -> Plugin */
  gtk_widget_activate_action (GTK_WIDGET (dialog), "win.page", "s", "mock");

  /* Close */
  gtk_widget_activate_action (GTK_WIDGET (dialog), "dialog.close", NULL);

  g_assert_null (dialog);
}

static void
test_preferences_dialog_rename (void)
{
  AdwDialog *dialog;

  dialog = g_object_new (VALENT_TYPE_TEST_SUBJECT,
                        NULL);
  g_object_add_weak_pointer (G_OBJECT (dialog), (gpointer)&dialog);

  adw_dialog_present (dialog, NULL);
  valent_test_await_pending ();

  /* Rename Dialog */
  gtk_widget_activate_action (GTK_WIDGET (dialog), "win.rename", NULL);
  valent_test_await_pending ();

  adw_dialog_force_close (dialog);
  valent_test_await_nullptr (&dialog);
}

int
main (int   argc,
      char *argv[])
{
  valent_test_ui_init (&argc, &argv, NULL);

  g_test_add_func ("/libvalent/ui/preferences-dialog",
                   test_preferences_dialog_basic);

  g_test_add_func ("/libvalent/ui/preferences-dialog/navigation",
                   test_preferences_dialog_navigation);

  g_test_add_func ("/libvalent/ui/preferences-dialog/rename",
                   test_preferences_dialog_rename);

  return g_test_run ();
}

