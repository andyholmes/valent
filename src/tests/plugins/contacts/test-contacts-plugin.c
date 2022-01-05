// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>
#include <libvalent-core.h>
#include <libvalent-contacts.h>
#include <libvalent-test.h>


static unsigned int n_contacts = 0;

static void
on_contact_added (ValentContactStore      *store,
                  const char              *uid,
                  EContact                *contact,
                  ValentTestPluginFixture *fixture)
{
  n_contacts++;

  if (n_contacts == 2)
    {
      n_contacts = 0;
      g_signal_handlers_disconnect_by_data (store, fixture);
      valent_test_plugin_fixture_quit (fixture);
    }
}

static void
contacts_plugin_fixture_set_up (ValentTestPluginFixture *fixture,
                                gconstpointer            user_data)
{
  valent_test_plugin_fixture_init (fixture, user_data);
  valent_test_plugin_fixture_init_settings (fixture, "contacts");

  g_settings_set_boolean (fixture->settings, "local-sync", TRUE);
  g_settings_set_string (fixture->settings, "local-uid", "test-device");
}

static void
test_contacts_plugin_request_contacts (ValentTestPluginFixture *fixture,
                                       gconstpointer            user_data)
{
  ValentContactStore *store;
  ValentDevice *device;
  g_autoslist (GObject) contacts = NULL;
  EBookQuery *query;
  g_autofree char *sexp = NULL;
  JsonNode *packet;

  /* Pop the initial request */
  device = valent_test_plugin_fixture_get_device (fixture);
  store = valent_contacts_ensure_store (valent_contacts_get_default (),
                                        valent_device_get_id (device),
                                        valent_device_get_name (device));

  g_signal_connect (store,
                    "contact-added",
                    G_CALLBACK (on_contact_added),
                    fixture);
  valent_test_plugin_fixture_connect (fixture, TRUE);

  /* Expect UIDs request */
  packet = valent_test_plugin_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.contacts.request_all_uids_timestamps");
  json_node_unref (packet);

  /* Expect UIDs request (GAction) */
  g_action_group_activate_action (valent_device_get_actions (device),
                                  "contacts-fetch",
                                  NULL);

  packet = valent_test_plugin_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.contacts.request_all_uids_timestamps");
  json_node_unref (packet);

  /* UIDs response */
  packet = valent_test_plugin_fixture_lookup_packet (fixture, "response-uids-timestamps");
  valent_test_plugin_fixture_handle_packet (fixture, packet);

  /* Expect vCard request */
  packet = valent_test_plugin_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.contacts.request_vcards_by_uid");
  json_node_unref (packet);

  /* vCard response */
  packet = valent_test_plugin_fixture_lookup_packet (fixture, "response-vcards");
  valent_test_plugin_fixture_handle_packet (fixture, packet);
  valent_test_plugin_fixture_run (fixture);

  query = e_book_query_vcard_field_exists (EVC_UID);
  sexp = e_book_query_to_string (query);
  e_book_query_unref (query);

  contacts = valent_contact_store_query_sync (store, sexp, NULL, NULL);
  g_assert_cmpuint (g_slist_length (contacts), ==, 2);

  while (g_main_context_iteration (NULL, FALSE))
    continue;
}

static void
test_contacts_plugin_provide_contacts (ValentTestPluginFixture *fixture,
                                       gconstpointer            user_data)
{
  JsonNode *packet;
  JsonArray *uids;

  valent_test_plugin_fixture_connect (fixture, TRUE);

  /* Expect UIDs request */
  packet = valent_test_plugin_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.contacts.request_all_uids_timestamps");
  json_node_unref (packet);

  /* UIDs request */
  packet = valent_test_plugin_fixture_lookup_packet (fixture, "request-all-uids-timestamps");
  valent_test_plugin_fixture_handle_packet (fixture, packet);

  /* Expect UIDs response */
  packet = valent_test_plugin_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.contacts.response_uids_timestamps");
  json_node_unref (packet);

  /* vCard request */
  packet = valent_test_plugin_fixture_lookup_packet (fixture, "request-vcards-by-uid");
  valent_test_plugin_fixture_handle_packet (fixture, packet);

  /* Expect vCard response */
  packet = valent_test_plugin_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.contacts.response_vcards");

  uids = json_object_get_array_member (valent_packet_get_body (packet), "uids");
  g_assert_cmpuint (json_array_get_length (uids), ==, 2);

  json_node_unref (packet);
}

static const char *schemas[] = {
  JSON_SCHEMA_DIR"/kdeconnect.contacts.request_all_uids_timestamps.json",
  JSON_SCHEMA_DIR"/kdeconnect.contacts.request_vcards_by_uid.json",
  JSON_SCHEMA_DIR"/kdeconnect.contacts.response_uids_timestamps.json",
  JSON_SCHEMA_DIR"/kdeconnect.contacts.response_vcards.json",
};

static void
test_contacts_plugin_fuzz (ValentTestPluginFixture *fixture,
                           gconstpointer            user_data)

{
  valent_test_plugin_fixture_connect (fixture, TRUE);
  g_test_log_set_fatal_handler (valent_test_mute_fuzzing, NULL);

  for (unsigned int s = 0; s < G_N_ELEMENTS (schemas); s++)
    valent_test_plugin_fixture_schema_fuzz (fixture, schemas[s]);
}

int
main (int   argc,
      char *argv[])
{
  const char *path = TEST_DATA_DIR"/plugin-contacts.json";

  g_test_init (&argc, &argv, G_TEST_OPTION_ISOLATE_DIRS, NULL);

  g_test_add ("/plugins/contacts/request-contacts",
              ValentTestPluginFixture, path,
              contacts_plugin_fixture_set_up,
              test_contacts_plugin_request_contacts,
              valent_test_plugin_fixture_clear);

  g_test_add ("/plugins/contacts/provide-contacts",
              ValentTestPluginFixture, path,
              contacts_plugin_fixture_set_up,
              test_contacts_plugin_provide_contacts,
              valent_test_plugin_fixture_clear);

#ifdef VALENT_TEST_FUZZ
  g_test_add ("/plugins/contacts/fuzz",
              ValentTestPluginFixture, path,
              valent_test_plugin_fixture_init,
              test_contacts_plugin_fuzz,
              valent_test_plugin_fixture_clear);
#endif

  return g_test_run ();
}
