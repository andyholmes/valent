// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <adwaita.h>
#include <valent.h>
#include <libvalent-test.h>

#include "valent-device-preferences-dialog.h"
#include "valent-preferences-command-editor.h"

static void
test_device_preferences_dialog (ValentTestFixture *fixture,
                                gconstpointer      user_data)
{
  AdwDialog *dialog;
  ValentDevice *device;
  PeasEngine *engine;
  PeasPluginInfo *info;

  dialog = g_object_new (VALENT_TYPE_DEVICE_PREFERENCES_DIALOG,
                         "device", fixture->device,
                         NULL);
  g_object_add_weak_pointer (G_OBJECT (dialog), (gpointer)&dialog);

  adw_dialog_present (dialog, NULL);
  valent_test_await_pending ();

  VALENT_TEST_CHECK ("GObject properties function correctly");
  g_object_get (dialog,
                "device", &device,
                NULL);
  g_assert_true (fixture->device == device);
  g_clear_object (&device);

  /* Unload/Load the plugin */
  engine = valent_get_plugin_engine ();
  info = peas_engine_get_plugin_info (engine, "mock");
  peas_engine_unload_plugin (engine, info);
  peas_engine_load_plugin (engine, info);

  adw_dialog_force_close (dialog);
  valent_test_await_nullptr (&dialog);
}

static void
test_preferences_command_editor (void)
{
  ValentPreferencesCommandEditor *editor = NULL;
  g_autoptr (GVariant) command = NULL;
  g_autoptr (GVariant) command_out = NULL;
  g_autofree char *uuid = NULL;
  g_autofree char *uuid_out = NULL;

  uuid = g_uuid_string_random ();
  command = g_variant_new_parsed ("{'name': <'%s'>, 'command': <'%s'>}",
                                  "Test Command", "echo \"foobar\"");
  g_variant_ref_sink (command);

  VALENT_TEST_CHECK ("Dialog can be constructed");
  editor = g_object_new (VALENT_TYPE_PREFERENCES_COMMAND_EDITOR,
                         "uuid",    uuid,
                         "command", command,
                         NULL);
  adw_dialog_present (ADW_DIALOG (editor), NULL);

  VALENT_TEST_CHECK ("GObject properties function correctly");
  g_object_get (editor,
                "uuid",    &uuid_out,
                "command", &command_out,
                NULL);
  g_assert_true (command_out == command);
  g_assert_cmpstr (uuid_out, ==, uuid);

  VALENT_TEST_CHECK ("Save action functions correctly");
  gtk_widget_activate_action (GTK_WIDGET (editor), "editor.save", NULL);
  g_assert_true (valent_preferences_command_editor_get_command (editor) != command);
  g_assert_cmpstr (valent_preferences_command_editor_get_uuid (editor), ==, uuid);

  VALENT_TEST_CHECK ("Remove action functions correctly");
  gtk_widget_activate_action (GTK_WIDGET (editor), "editor.remove", NULL);
  g_assert_null (valent_preferences_command_editor_get_command (editor));
  g_assert_cmpstr (valent_preferences_command_editor_get_uuid (editor), ==, uuid);

  adw_dialog_force_close (ADW_DIALOG (editor));
}

int
main (int   argc,
      char *argv[])
{
  const char *path = "plugin-mock.json";

  valent_test_ui_init (&argc, &argv, NULL);

  g_test_add ("/plugins/gnome/device-preferences/dialog",
              ValentTestFixture, path,
              valent_test_fixture_init,
              test_device_preferences_dialog,
              valent_test_fixture_clear);

  g_test_add_func ("/plugins/gnome/preferences-command-editor",
                   test_preferences_command_editor);

  return g_test_run ();
}

