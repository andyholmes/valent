// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>
#include <libtracker-sparql/tracker-sparql.h>
#include <valent.h>
#include <libvalent-test.h>


typedef struct
{
} ContactsComponentFixture;

static void
contacts_component_fixture_set_up (ContactsComponentFixture *fixture,
                                   gconstpointer             user_data)
{
}

static void
contacts_component_fixture_tear_down (ContactsComponentFixture *fixture,
                                      gconstpointer             user_data)
{
}

static void
test_contacts_component_adapter (ContactsComponentFixture *fixture,
                                 gconstpointer             user_data)
{
  PeasEngine *engine;
  PeasPluginInfo *plugin_info;
  g_autoptr (GObject) adapter = NULL;
  g_autoptr (TrackerSparqlConnection) connection = NULL;
  unsigned int n_items = 0;

  engine = valent_get_plugin_engine ();
  plugin_info = peas_engine_get_plugin_info (engine, "mock");

  VALENT_TEST_CHECK ("Adapter can be constructed");
  adapter = peas_engine_create_extension (engine,
                                          plugin_info,
                                          VALENT_TYPE_CONTACTS_ADAPTER,
                                          "iri",    "urn:valent:contacts:mock",
                                          "object", NULL,
                                          NULL);

  VALENT_TEST_CHECK ("GObject properties function correctly");
  g_object_get (adapter,
                "connection", &connection,
                NULL);
  g_assert_true (TRACKER_IS_SPARQL_CONNECTION (connection));

  VALENT_TEST_CHECK ("Adapter implements GListModel correctly");
  g_assert_true (G_LIST_MODEL (adapter));
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (adapter)), >, 0);
  g_assert_true (g_list_model_get_item_type (G_LIST_MODEL (adapter)) == G_TYPE_LIST_MODEL);

  n_items = g_list_model_get_n_items (G_LIST_MODEL (adapter));
  for (unsigned int i = 0; i < n_items; i++)
    {
      g_autoptr (GListModel) item = g_list_model_get_item (G_LIST_MODEL (adapter), i);
      g_assert_true (G_IS_LIST_MODEL (item));
    }

  VALENT_TEST_CHECK ("Adapter detects contact lists added to the graph");
  g_action_group_activate_action (G_ACTION_GROUP (adapter),
                                  "add-list",
                                  g_variant_new_string ("temporary"));
  valent_test_await_signal (adapter, "items-changed");
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (adapter)), ==, n_items + 1);

  VALENT_TEST_CHECK ("Adapter detects contact lists removed from the graph");
  g_action_group_activate_action (G_ACTION_GROUP (adapter),
                                  "remove-list",
                                  g_variant_new_string ("temporary"));
  valent_test_await_signal (adapter, "items-changed");
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (adapter)), ==, n_items);
}

static void
test_contacts_component_contact_list (ContactsComponentFixture *fixture,
                                      gconstpointer             user_data)
{
  PeasEngine *engine;
  PeasPluginInfo *plugin_info;
  g_autoptr (GObject) adapter = NULL;
  g_autoptr (GListModel) list = NULL;
  g_autoptr (TrackerSparqlConnection) connection = NULL;
  unsigned int n_items = 0;

  engine = valent_get_plugin_engine ();
  plugin_info = peas_engine_get_plugin_info (engine, "mock");
  adapter = peas_engine_create_extension (engine,
                                          plugin_info,
                                          VALENT_TYPE_CONTACTS_ADAPTER,
                                          "iri",    "urn:valent:contacts:mock",
                                          "object", NULL,
                                          NULL);

  list = g_list_model_get_item (G_LIST_MODEL (adapter), 0);

  VALENT_TEST_CHECK ("GObject properties function correctly");
  g_object_get (list,
                "connection", &connection,
                NULL);
  g_assert_true (TRACKER_IS_SPARQL_CONNECTION (connection));

  VALENT_TEST_CHECK ("Contact list implements GListModel correctly");
  g_assert_true (G_IS_LIST_MODEL (list));
  g_assert_cmpuint (g_list_model_get_n_items (list), >, 0);
  g_assert_true (g_list_model_get_item_type (list) == E_TYPE_CONTACT);

  n_items = g_list_model_get_n_items (list);
  for (unsigned int i = 0; i < n_items; i++)
    {
      g_autoptr (EContact) item = g_list_model_get_item (list, i);
      g_assert_true (E_IS_CONTACT (item));
    }

  VALENT_TEST_CHECK ("Contact list detects contacts added to the graph");
  g_action_group_activate_action (G_ACTION_GROUP (adapter),
                                  "add-contact",
                                  g_variant_new_string ("BEGIN:VCARD\nVERSION:2.1\nUID:001\nFN:Sepideh\nTEL:123-456-7897\nEND:VCARD\n"));
  valent_test_await_signal (list, "items-changed");
  g_assert_cmpuint (g_list_model_get_n_items (list), ==, n_items + 1);

  VALENT_TEST_CHECK ("Contact list detects contacts removed from the graph");
  g_action_group_activate_action (G_ACTION_GROUP (adapter),
                                  "remove-contact",
                                  g_variant_new_string ("001"));
  valent_test_await_signal (list, "items-changed");
  g_assert_cmpuint (g_list_model_get_n_items (list), ==, n_items);
}

static void
test_contacts_component_self (ContactsComponentFixture *fixture,
                              gconstpointer             user_data)
{
  ValentContacts *contacts = valent_contacts_get_default ();
  unsigned int n_items = 0;

  VALENT_TEST_CHECK ("Component implements GListModel correctly");
  g_assert_true (G_LIST_MODEL (contacts));
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (contacts)), >, 0);
  g_assert_true (g_list_model_get_item_type (G_LIST_MODEL (contacts)) == VALENT_TYPE_CONTACTS_ADAPTER);

  n_items = g_list_model_get_n_items (G_LIST_MODEL (contacts));
  for (unsigned int i = 0; i < n_items; i++)
    {
      g_autoptr (ValentContactsAdapter) item = NULL;

      item = g_list_model_get_item (G_LIST_MODEL (contacts), i);
      g_assert_true (VALENT_IS_CONTACTS_ADAPTER (item));
    }

  v_await_finalize_object (contacts);
}

int
main (int   argc,
      char *argv[])
{
  valent_test_init (&argc, &argv, NULL);

  g_test_add ("/libvalent/contacts/adapter",
              ContactsComponentFixture, NULL,
              contacts_component_fixture_set_up,
              test_contacts_component_adapter,
              contacts_component_fixture_tear_down);

  g_test_add ("/libvalent/contacts/list",
              ContactsComponentFixture, NULL,
              contacts_component_fixture_set_up,
              test_contacts_component_contact_list,
              contacts_component_fixture_tear_down);

  g_test_add ("/libvalent/contacts/self",
              ContactsComponentFixture, NULL,
              contacts_component_fixture_set_up,
              test_contacts_component_self,
              contacts_component_fixture_tear_down);

  return g_test_run ();
}
