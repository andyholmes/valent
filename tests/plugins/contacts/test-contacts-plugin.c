// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>
#include <valent.h>
#include <libvalent-test.h>


static void
on_adapter_changed (GListModel    *list,
                    unsigned int   position,
                    unsigned int   removed,
                    unsigned int   added,
                    GListModel   **addressbook_out)
{
  if (position == 0 && removed == 0 && added == 1)
    {
      g_signal_handlers_disconnect_by_data (list, addressbook_out);
      *addressbook_out = g_list_model_get_item (list, 0);
    }
}

static void
on_contact_list_changed (GListModel   *list,
                         unsigned int  position,
                         unsigned int  removed,
                         unsigned int  added,
                         gboolean     *done)
{
  if (g_list_model_get_n_items (list) == 3)
    {
      g_signal_handlers_disconnect_by_data (list, done);
      *done = TRUE;
    }
}

static void
contacts_plugin_fixture_set_up (ValentTestFixture *fixture,
                                gconstpointer      user_data)
{
  valent_test_fixture_init (fixture, user_data);

  g_settings_set_boolean (fixture->settings, "local-sync", TRUE);
  g_settings_set_string (fixture->settings, "local-uid", "urn:valent:contacts:mock");
}

static void
contacts_plugin_fixture_clear (ValentTestFixture *fixture,
                               gconstpointer      user_data)
{
  valent_test_fixture_clear (fixture, user_data);

  /* NOTE: we need to finalize the singletons between tests */
  v_assert_finalize_object (valent_contacts_get_default ());
}

static void
test_contacts_plugin_basic (ValentTestFixture *fixture,
                            gconstpointer      user_data)
{
  GActionGroup *actions = G_ACTION_GROUP (fixture->device);
  JsonNode *packet;

  VALENT_TEST_CHECK ("Plugin has expected actions");
  g_assert_true (g_action_group_has_action (actions, "contacts.fetch"));

  valent_test_fixture_connect (fixture, TRUE);

  VALENT_TEST_CHECK ("Plugin requests a list of UIDs on connect");
  valent_test_fixture_connect (fixture, TRUE);

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.contacts.request_all_uids_timestamps");
  json_node_unref (packet);

  VALENT_TEST_CHECK ("Plugin actions are enabled when connected");
  g_assert_true (g_action_group_get_action_enabled (actions, "contacts.fetch"));

  VALENT_TEST_CHECK ("Plugin action `contacts.fetch` sends a request for contacts");
  g_action_group_activate_action (actions, "contacts.fetch", NULL);

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.contacts.request_all_uids_timestamps");
  json_node_unref (packet);
}

static void
test_contacts_plugin_request_contacts (ValentTestFixture *fixture,
                                       gconstpointer      user_data)
{
  g_autoptr (ValentContactsAdapter) adapter = NULL;
  g_autoptr (GListModel) addressbook = NULL;
  gboolean done = FALSE;
  JsonNode *packet;

  VALENT_TEST_CHECK ("Plugin requests a list of UIDs on connect");
  valent_test_fixture_connect (fixture, TRUE);

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.contacts.request_all_uids_timestamps");
  json_node_unref (packet);

  packet = valent_test_fixture_lookup_packet (fixture, "response-uids-timestamps");
  valent_test_fixture_handle_packet (fixture, packet);

  VALENT_TEST_CHECK ("Plugin responds to a list of UIDs with a request for vCard data");
  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.contacts.request_vcards_by_uid");
  json_node_unref (packet);

  packet = valent_test_fixture_lookup_packet (fixture, "response-vcards");
  valent_test_fixture_handle_packet (fixture, packet);

  VALENT_TEST_CHECK ("Plugin adds contact vCards to the contact store");
  adapter = g_list_model_get_item (G_LIST_MODEL (valent_contacts_get_default ()), 1);
  addressbook = g_list_model_get_item (G_LIST_MODEL (adapter), 0);
  if (addressbook == NULL)
    {
      g_signal_connect (adapter,
                        "items-changed",
                        G_CALLBACK (on_adapter_changed),
                        &addressbook);
      valent_test_await_pointer (&addressbook);
    }

  g_signal_connect (addressbook,
                    "items-changed",
                    G_CALLBACK (on_contact_list_changed),
                    &done);
  valent_test_await_boolean (&done);
}

static void
test_contacts_plugin_provide_contacts (ValentTestFixture *fixture,
                                       gconstpointer      user_data)
{
  JsonNode *packet;
  JsonNode *request;
  JsonArray *uids;
  unsigned int n_uids;

  VALENT_TEST_CHECK ("Plugin requests a list of UIDs on connect");
  valent_test_fixture_connect (fixture, TRUE);

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.contacts.request_all_uids_timestamps");
  json_node_unref (packet);

  VALENT_TEST_CHECK ("Plugin returns a list of contact UIDs when requested");
  packet = valent_test_fixture_lookup_packet (fixture, "request-all-uids-timestamps");
  valent_test_fixture_handle_packet (fixture, packet);

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.contacts.response_uids_timestamps");
  g_assert_true (valent_packet_get_array (packet, "uids", &uids));
  n_uids = json_array_get_length (uids);

  VALENT_TEST_CHECK ("Plugin returns a list of contacts when requested");
  request = valent_packet_new ("kdeconnect.contacts.request_vcards_by_uid");
  json_object_set_array_member (valent_packet_get_body (request),
                                "uids", json_array_ref (uids));
  valent_test_fixture_handle_packet (fixture, request);
  json_node_unref (packet);
  json_node_unref (request);

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.contacts.response_vcards");
  uids = json_object_get_array_member (valent_packet_get_body (packet), "uids");
  g_assert_cmpuint (json_array_get_length (uids), ==, n_uids);
  json_node_unref (packet);
}

static const char *schemas[] = {
  "/tests/kdeconnect.contacts.request_all_uids_timestamps.json",
  "/tests/kdeconnect.contacts.request_vcards_by_uid.json",
  "/tests/kdeconnect.contacts.response_uids_timestamps.json",
  "/tests/kdeconnect.contacts.response_vcards.json",
};

static void
test_contacts_plugin_fuzz (ValentTestFixture *fixture,
                           gconstpointer      user_data)

{
  valent_test_fixture_connect (fixture, TRUE);
  g_test_log_set_fatal_handler (valent_test_mute_fuzzing, NULL);

  for (size_t s = 0; s < G_N_ELEMENTS (schemas); s++)
    valent_test_fixture_schema_fuzz (fixture, schemas[s]);
}

int
main (int   argc,
      char *argv[])
{
  const char *path = "plugin-contacts.json";

  valent_test_init (&argc, &argv, NULL);

  g_test_add ("/plugins/contacts/basic",
              ValentTestFixture, path,
              contacts_plugin_fixture_set_up,
              test_contacts_plugin_basic,
              contacts_plugin_fixture_clear);

  g_test_add ("/plugins/contacts/request-contacts",
              ValentTestFixture, path,
              contacts_plugin_fixture_set_up,
              test_contacts_plugin_request_contacts,
              contacts_plugin_fixture_clear);

  g_test_add ("/plugins/contacts/provide-contacts",
              ValentTestFixture, path,
              contacts_plugin_fixture_set_up,
              test_contacts_plugin_provide_contacts,
              contacts_plugin_fixture_clear);

  g_test_add ("/plugins/contacts/fuzz",
              ValentTestFixture, path,
              valent_test_fixture_init,
              test_contacts_plugin_fuzz,
              contacts_plugin_fixture_clear);

  return g_test_run ();
}
