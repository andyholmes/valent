// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <valent.h>
#include <libvalent-test.h>


static int stage = 0;

static gboolean
basic_timeout_cb (gpointer data)
{
  GApplication *application = G_APPLICATION (data);

  g_application_quit (application);

  return G_SOURCE_REMOVE;
}

static void
test_application_basic (void)
{
  g_autoptr (GApplication) application = NULL;
  int ret = 0;

  VALENT_TEST_CHECK ("Application can be started, process events, and exit");
  application = _valent_application_new ();

  g_idle_add (basic_timeout_cb, application);

  ret = g_application_run (G_APPLICATION (application), 0, NULL);

  g_assert_cmpint (ret, ==, 0);
}

static gboolean
actions_timeout_cb (gpointer data)
{
  GActionGroup *actions = G_ACTION_GROUP (data);

  switch (stage++)
    {
    case 0:
      VALENT_TEST_CHECK ("Application action `app.quit` exits cleanly");
      g_action_group_activate_action (actions, "quit", NULL);
      return G_SOURCE_REMOVE;

    default:
      return G_SOURCE_REMOVE;
    }
}

static void
test_application_actions (void)
{
  g_autoptr (GApplication) application = NULL;
  int ret = 0;

  VALENT_TEST_CHECK ("Application can be started");
  application = _valent_application_new ();

  stage = 0;
  g_idle_add (actions_timeout_cb, application);

  ret = g_application_run (G_APPLICATION (application), 0, NULL);

  g_assert_cmpint (ret, ==, 0);
}

static gboolean
plugins_timeout_cb (gpointer data)
{
  GApplication *application = G_APPLICATION (data);
  g_autoptr (GSettings) settings = NULL;
  g_autoptr (GPtrArray) files = NULL;
  PeasEngine *engine;
  PeasPluginInfo *info;

  engine = valent_get_plugin_engine ();
  info = peas_engine_get_plugin_info (engine, "mock");
  settings = valent_test_mock_settings ("application");

  switch (stage++)
    {
    case 0:
      VALENT_TEST_CHECK ("Application handles plugins being unloaded");
      peas_engine_unload_plugin (engine, info);
      return G_SOURCE_CONTINUE;

    case 1:
      VALENT_TEST_CHECK ("Application handles plugins being loaded");
      peas_engine_load_plugin (engine, info);
      return G_SOURCE_CONTINUE;

    case 2:
      VALENT_TEST_CHECK ("Application handles plugins being disabled");
      g_settings_set_boolean (settings, "enabled", FALSE);
      return G_SOURCE_CONTINUE;

    case 3:
      VALENT_TEST_CHECK ("Application handles plugins being enabled");
      g_settings_set_boolean (settings, "enabled", TRUE);
      return G_SOURCE_CONTINUE;

    case 4:
      VALENT_TEST_CHECK ("Application activates `open()` on plugins");
      files = g_ptr_array_new_with_free_func (g_object_unref);
      g_ptr_array_add (files, g_file_new_for_path ("."));
      g_application_open (application, *files->pdata, files->len, "");
      return G_SOURCE_CONTINUE;

    default:
      VALENT_TEST_CHECK ("Application exits cleanly");
      g_application_quit (application);
      return G_SOURCE_REMOVE;
    }
}

static void
test_application_plugins (void)
{
  g_autoptr (GApplication) application = NULL;
  int ret = 0;

  application = _valent_application_new ();

  stage = 0;
  g_idle_add (plugins_timeout_cb, application);

  ret = g_application_run (G_APPLICATION (application), 0, NULL);

  g_assert_cmpint (ret, ==, 0);
}

int
main (int   argc,
      char *argv[])
{
  valent_test_init (&argc, &argv, NULL);

  g_set_application_name ("Valent");

  g_test_add_func ("/libvalent/core/application/basic",
                   test_application_basic);

  g_test_add_func ("/libvalent/core/application/actions",
                   test_application_actions);

  g_test_add_func ("/libvalent/core/application/plugins",
                   test_application_plugins);

  return g_test_run ();
}
