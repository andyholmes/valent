// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <valent.h>
#include <libvalent-test.h>

#define VALENT_TYPE_TEST_SUBJECT (g_type_from_name ("ValentDevicePreferencesDialog"))


static void
test_device_preference_dialog_basic (ValentTestFixture *fixture,
                                     gconstpointer      user_data)
{
  AdwDialog *dialog;
  ValentDevice *device;
  PeasEngine *engine;
  PeasPluginInfo *info;

  dialog = g_object_new (VALENT_TYPE_TEST_SUBJECT,
                         "device", fixture->device,
                         NULL);
  g_object_add_weak_pointer (G_OBJECT (dialog), (gpointer)&dialog);

  adw_dialog_present (dialog, NULL);
  valent_test_await_pending ();

  VALENT_TEST_CHECK ("GObject properties function correctly");
  g_object_get (window,
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

int
main (int   argc,
      char *argv[])
{
  const char *path = "plugin-mock.json";

  valent_test_ui_init (&argc, &argv, NULL);

  g_test_add ("/libvalent/ui/device-preferences-dialog/basic",
              ValentTestFixture, path,
              valent_test_fixture_init,
              test_device_preference_dialog_basic,
              valent_test_fixture_clear);

  return g_test_run ();
}

