// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gtk/gtk.h>
#include <valent.h>
#include <libvalent-test.h>

#include "test-gnome-common.h"

#define VALENT_TYPE_TEST_SUBJECT (g_type_from_name ("ValentInputRemote"))


static void
test_input_remote (void)
{
  PeasEngine *engine;
  PeasPluginInfo *plugin_info;
  g_autoptr (ValentContext) context = NULL;
  GtkWindow *remote = NULL;
  GListStore *list = NULL;
  g_autoptr (GListStore) adapters = NULL;
  g_autoptr (GObject) adapter = NULL;

  engine = valent_get_plugin_engine ();
  plugin_info = peas_engine_get_plugin_info (engine, "mock");
  context = valent_context_new (NULL, "plugin", "mock");

  VALENT_TEST_CHECK ("Window can be constructed");
  list = g_list_store_new (VALENT_TYPE_INPUT_ADAPTER);
  remote = g_object_new (VALENT_TYPE_TEST_SUBJECT,
                         "adapters", list,
                         NULL);

  VALENT_TEST_CHECK ("GObject properties function correctly");
  g_object_get (remote,
                "adapters", &adapters,
                NULL);
  g_assert_true (list == adapters);
  g_clear_object (&adapters);

  VALENT_TEST_CHECK ("Window can be presented");
  gtk_window_present (remote);
  valent_test_await_pending ();

  VALENT_TEST_CHECK ("Window can add adapters");
  adapter = peas_engine_create_extension (engine,
                                          plugin_info,
                                          VALENT_TYPE_INPUT_ADAPTER,
                                          "iri",     "urn:valent:input:remote",
                                          "parent",  NULL,
                                          "context", context,
                                          NULL);
  g_list_store_append (list, adapter);

  VALENT_TEST_CHECK ("Window can remove adapters");
  g_list_store_remove (list, 0);

  VALENT_TEST_CHECK ("Window can be destroyed");
  gtk_window_destroy (remote);
  valent_test_await_pending ();
}

int
main (int   argc,
      char *argv[])
{
  valent_test_gnome_init (&argc, &argv, NULL);

  g_test_add_func ("/plugins/gnome/input-remote",
                   test_input_remote);

  return g_test_run ();
}

