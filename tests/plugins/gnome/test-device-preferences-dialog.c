// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <adwaita.h>
#include <valent.h>
#include <libvalent-test.h>

#include "valent-device-preferences-dialog.h"
#include "valent-device-preferences-battery.h"
#include "valent-device-preferences-clipboard.h"
#include "valent-device-preferences-commands.h"
#include "valent-device-preferences-commands-editor.h"
#include "valent-device-preferences-connectivity.h"
#include "valent-device-preferences-contacts.h"
#include "valent-device-preferences-notification.h"
#include "valent-device-preferences-sftp.h"
#include "valent-device-preferences-share.h"
#include "valent-device-preferences-telephony.h"

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

typedef struct
{
  const char *module;
  GType gtype;
} PreferencesTest;

static void
test_device_preferences_group_init (ValentTestFixture *fixture,
                                    gconstpointer      user_data)
{
  PreferencesTest *test = (PreferencesTest *)user_data;
  g_autofree char *path = g_strdup_printf ("plugin-%s.json", test->module);

  valent_test_fixture_init (fixture, path);
}

static void
test_device_preferences_group (ValentTestFixture *fixture,
                               gconstpointer      user_data)
{
  PreferencesTest *test = (PreferencesTest *)user_data;
  PeasEngine *engine;
  PeasPluginInfo *info;
  g_autoptr (PeasPluginInfo) info_out = NULL;
  GtkWidget *prefs;

  VALENT_TEST_CHECK ("Plugin can be constructed");
  engine = valent_get_plugin_engine ();
  info = peas_engine_get_plugin_info (engine, test->module);
  prefs = g_object_new (test->gtype,
                        "source",      valent_data_source_get_local_default (),
                        "plugin-info", info,
                        NULL);
  g_object_ref_sink (prefs);

  VALENT_TEST_CHECK ("GObject properties function correctly");
  g_object_get (prefs,
                "plugin-info", &info_out,
                NULL);
  g_assert_true (info == info_out);

  g_object_unref (prefs);
}

static void
on_command_changed (ValentRuncommandEditor  *editor,
                    GParamSpec              *pspec,
                    GVariant               **out)
{
  if (out)
    *out = valent_runcommand_editor_get_command (editor);
}

static void
test_device_preferences_commands_editor (void)
{
  g_autoptr (ValentRuncommandEditor) editor = NULL;
  g_autoptr (GVariant) command = NULL;
  g_autoptr (GVariant) test_command = NULL;
  g_autofree char *uuid = NULL;
  GVariant *out = NULL;

#if 0
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
#endif

  test_command = g_variant_new_parsed ("{'name': <'%s'>, 'command': <'%s'>}",
                                       "Test Command", "echo \"foobar\"");
  g_variant_ref_sink (test_command);

  editor = g_object_new (VALENT_TYPE_RUNCOMMAND_EDITOR,
                         "uuid",    "test",
                         "command", test_command,
                         NULL);
  g_signal_connect (editor,
                    "notify::command",
                    G_CALLBACK (on_command_changed),
                    &out);
  gtk_window_present (GTK_WINDOW (editor));

  /* Properties */
  g_object_get (editor,
                "uuid",    &uuid,
                "command", &command,
                NULL);

  g_assert_true (command == test_command);
  g_assert_cmpstr (uuid, ==, "test");
  g_clear_pointer (&command, g_variant_unref);
  g_clear_pointer (&uuid, g_free);

  VALENT_TEST_CHECK ("Edit operation can be cancelled");
  gtk_widget_activate_action (GTK_WIDGET (editor), "editor.cancel", NULL);
  g_assert_true (out == test_command);
  g_assert_true (valent_runcommand_editor_get_command (editor) == test_command);
  g_assert_cmpstr (valent_runcommand_editor_get_uuid (editor), ==, "test");

  /* Save */
  gtk_widget_activate_action (GTK_WIDGET (editor), "editor.save", NULL);
  g_assert_true (out != test_command);
  g_assert_true (valent_runcommand_editor_get_command (editor) != test_command);
  g_assert_cmpstr (valent_runcommand_editor_get_uuid (editor), ==, "test");

  /* Remove */
  gtk_widget_activate_action (GTK_WIDGET (editor), "editor.remove", NULL);
  g_assert_true (out == NULL);
  g_assert_null (valent_runcommand_editor_get_command (editor));
  g_assert_cmpstr (valent_runcommand_editor_get_uuid (editor), ==, "test");
}

int
main (int   argc,
      char *argv[])
{
  const char *path = "plugin-mock.json";
  const PreferencesTest plugin_tests[] = {
    {
      .module = "battery",
      .gtype = VALENT_TYPE_BATTERY_PREFERENCES,
    },
    {
      .module = "clipboard",
      .gtype = VALENT_TYPE_CLIPBOARD_PREFERENCES,
    },
    {
      .module = "runcommand",
      .gtype = VALENT_TYPE_RUNCOMMAND_PREFERENCES,
    },
    {
      .module = "connectivity_report",
      .gtype = VALENT_TYPE_CONNECTIVITY_REPORT_PREFERENCES,
    },
    {
      .module = "contacts",
      .gtype = VALENT_TYPE_CONTACTS_PREFERENCES,
    },
    {
      .module = "notification",
      .gtype = VALENT_TYPE_NOTIFICATION_PREFERENCES,
    },
    {
      .module = "sftp",
      .gtype = VALENT_TYPE_SFTP_PREFERENCES,
    },
    {
      .module = "share",
      .gtype = VALENT_TYPE_SHARE_PREFERENCES,
    },
    {
      .module = "telephony",
      .gtype = VALENT_TYPE_TELEPHONY_PREFERENCES,
    },
  };

  valent_test_ui_init (&argc, &argv, NULL);

  g_test_add ("/plugins/gnome/device-preferences/dialog",
              ValentTestFixture, path,
              valent_test_fixture_init,
              test_device_preferences_dialog,
              valent_test_fixture_clear);

  for (size_t i = 0; i < G_N_ELEMENTS (plugin_tests); i++)
    {
      g_autofree char *test_path = NULL;

      test_path = g_strdup_printf ("/plugins/gnome/device-preferences/%s",
                                   plugin_tests[i].module);
      g_test_add (test_path,
                  ValentTestFixture, &plugin_tests[i],
                  test_device_preferences_group_init,
                  test_device_preferences_group,
                  valent_test_fixture_clear);
    }

  g_test_add_func ("/plugins/gnome/device-preferences/commands-editor",
                   test_device_preferences_commands_editor);

  return g_test_run ();
}

