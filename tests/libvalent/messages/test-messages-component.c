// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>
#include <libtracker-sparql/tracker-sparql.h>
#include <valent.h>
#include <libvalent-test.h>


typedef struct
{
} MessagesComponentFixture;

static void
messages_component_fixture_set_up (MessagesComponentFixture *fixture,
                                   gconstpointer             user_data)
{
}

static void
messages_component_fixture_tear_down (MessagesComponentFixture *fixture,
                                      gconstpointer             user_data)
{
}

static void
test_messages_component_adapter (MessagesComponentFixture *fixture,
                                 gconstpointer             user_data)
{
  PeasEngine *engine;
  PeasPluginInfo *plugin_info;
  g_autoptr (ValentContext) context = NULL;
  g_autoptr (GObject) adapter = NULL;
  g_autoptr (TrackerSparqlConnection) connection = NULL;
  unsigned int n_items = 0;

  engine = valent_get_plugin_engine ();
  plugin_info = peas_engine_get_plugin_info (engine, "mock");
  context = valent_context_new (NULL, "plugin", "mock");

  VALENT_TEST_CHECK ("Adapter can be constructed");
  adapter = peas_engine_create_extension (engine,
                                          plugin_info,
                                          VALENT_TYPE_MESSAGES_ADAPTER,
                                          "iri",     "urn:valent:messages:mock",
                                          "source",  NULL,
                                          "context", context,
                                          NULL);

  VALENT_TEST_CHECK ("GObject properties function correctly");
  g_object_get (adapter,
                "connection", &connection,
                NULL);
  g_assert_true (TRACKER_IS_SPARQL_CONNECTION (connection));

  VALENT_TEST_CHECK ("Adapter implements GListModel correctly");
  g_assert_true (G_LIST_MODEL (adapter));
  g_assert_true (g_list_model_get_item_type (G_LIST_MODEL (adapter)) == G_TYPE_LIST_MODEL);

  n_items = g_list_model_get_n_items (G_LIST_MODEL (adapter));
  for (unsigned int i = 0; i < n_items; i++)
    {
      g_autoptr (GListModel) item = g_list_model_get_item (G_LIST_MODEL (adapter), i);
      g_assert_true (G_IS_LIST_MODEL (item));
    }

  VALENT_TEST_CHECK ("Adapter detects message lists added to the graph");
  g_action_group_activate_action (G_ACTION_GROUP (adapter),
                                  "add-message",
                                  g_variant_new ("(xx)", 4, 1));
  valent_test_await_signal (adapter, "items-changed");
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (adapter)), ==, n_items + 1);

  VALENT_TEST_CHECK ("Adapter detects message lists removed from the graph");
  g_action_group_activate_action (G_ACTION_GROUP (adapter),
                                  "remove-list",
                                  g_variant_new ("(xx)", 4, -1));
  valent_test_await_signal (adapter, "items-changed");
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (adapter)), ==, n_items);
}

static void
test_messages_component_message_list (MessagesComponentFixture *fixture,
                                      gconstpointer             user_data)
{
  PeasEngine *engine;
  PeasPluginInfo *plugin_info;
  g_autoptr (ValentContext) context = NULL;
  g_autoptr (GObject) adapter = NULL;
  g_autoptr (GListModel) list = NULL;
  g_autoptr (TrackerSparqlConnection) connection = NULL;
  unsigned int n_items = 0;

  engine = valent_get_plugin_engine ();
  plugin_info = peas_engine_get_plugin_info (engine, "mock");
  context = valent_context_new (NULL, "plugin", "mock");

  VALENT_TEST_CHECK ("Adapter can be constructed");
  adapter = peas_engine_create_extension (engine,
                                          plugin_info,
                                          VALENT_TYPE_MESSAGES_ADAPTER,
                                          "iri",     "urn:valent:messages:mock",
                                          "source",  NULL,
                                          "context", context,
                                          NULL);

  // HACK: to address the lazy-load hack in ValentMessageThread
  list = g_list_model_get_item (G_LIST_MODEL (adapter), 0);
  while (g_list_model_get_n_items (G_LIST_MODEL (list)) != 2)
    g_main_context_iteration (NULL, FALSE);

  VALENT_TEST_CHECK ("GObject properties function correctly");
  g_object_get (list,
                "connection", &connection,
                NULL);
  g_assert_true (TRACKER_IS_SPARQL_CONNECTION (connection));

  VALENT_TEST_CHECK ("Message list implements GListModel correctly");
  g_assert_true (G_IS_LIST_MODEL (list));
  g_assert_true (g_list_model_get_item_type (list) == VALENT_TYPE_MESSAGE);

  n_items = g_list_model_get_n_items (list);
  for (unsigned int i = 0; i < n_items; i++)
    {
      g_autoptr (GObject) item = g_list_model_get_item (list, i);
      g_assert_true (VALENT_IS_MESSAGE (item));
    }

  VALENT_TEST_CHECK ("Message list detects messages added to the graph");
  g_action_group_activate_action (G_ACTION_GROUP (adapter),
                                  "add-message",
                                  g_variant_new ("(xx)", 38, 3316));
  valent_test_await_signal (list, "items-changed");
  g_assert_cmpuint (g_list_model_get_n_items (list), ==, n_items + 1);

  VALENT_TEST_CHECK ("Message list detects messages removed from the graph");
  g_action_group_activate_action (G_ACTION_GROUP (adapter),
                                  "remove-message",
                                  g_variant_new ("(xx)", 38, 3316));
  valent_test_await_signal (list, "items-changed");
  g_assert_cmpuint (g_list_model_get_n_items (list), ==, n_items);
}

static void
test_messages_component_self (MessagesComponentFixture *fixture,
                              gconstpointer             user_data)
{
  ValentMessages *messages = valent_messages_get_default ();
  unsigned int n_items = 0;

  VALENT_TEST_CHECK ("Component implements GListModel correctly");
  g_assert_true (G_LIST_MODEL (messages));
  g_assert_true (g_list_model_get_item_type (G_LIST_MODEL (messages)) == VALENT_TYPE_MESSAGES_ADAPTER);

  n_items = g_list_model_get_n_items (G_LIST_MODEL (messages));
  for (unsigned int i = 0; i < n_items; i++)
    {
      g_autoptr (ValentMessagesAdapter) item = NULL;

      item = g_list_model_get_item (G_LIST_MODEL (messages), i);
      g_assert_true (VALENT_IS_MESSAGES_ADAPTER (item));
    }

  v_await_finalize_object (messages);
}

int
main (int   argc,
      char *argv[])
{
  valent_test_init (&argc, &argv, NULL);

  g_test_add ("/libvalent/messages/adapter",
              MessagesComponentFixture, NULL,
              messages_component_fixture_set_up,
              test_messages_component_adapter,
              messages_component_fixture_tear_down);

  g_test_add ("/libvalent/messages/list",
              MessagesComponentFixture, NULL,
              messages_component_fixture_set_up,
              test_messages_component_message_list,
              messages_component_fixture_tear_down);

  g_test_add ("/libvalent/messages/self",
              MessagesComponentFixture, NULL,
              messages_component_fixture_set_up,
              test_messages_component_self,
              messages_component_fixture_tear_down);

  return g_test_run ();
}
