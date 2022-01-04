// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>
#include <libvalent-core.h>
#include <libvalent-contacts.h>
#include <libvalent-test.h>


typedef struct
{
  ValentContacts             *contacts;
  ValentContactStoreProvider *provider;
  ValentContactStore         *store;
  EContact                   *contact;
  GMainLoop                  *loop;
  gpointer                    emitter;
  gpointer                    emitted;
  gpointer                    result;
} ContactsComponentFixture;


typedef struct
{
  const char *original;
  const char *normalized;
} PhoneNumber;

static const PhoneNumber numbers[] = {
  {"754-3010",         "7543010"},     // Local
  {"(541) 754-3010",   "5417543010"},  // Domestic
  {"+1-541-754-3010",  "15417543010"}, // International
  {"1-541-754-3010",   "15417543010"}, // International (US)
  {"001-541-754-3010", "15417543010"}  // International (EU)
};

static const char vcard[] =
 "BEGIN:VCARD\n"
 "VERSION:2.1\n"
 "FN:Test Contact\n"
 "TEL;CELL:123-456-7890\n"
 "END:VCARD\n";


static void
on_store_added (GObject                  *object,
                ValentContactStore       *store,
                ContactsComponentFixture *fixture)
{
  g_assert_true (VALENT_IS_CONTACT_STORE (store));

  fixture->emitter = object;
  fixture->emitted = store;
}

static void
on_store_removed (GObject                  *object,
                  ValentContactStore       *store,
                  ContactsComponentFixture *fixture)
{
  g_assert_true (VALENT_IS_CONTACT_STORE (store));

  fixture->emitter = object;
  fixture->emitted = store;
}

static void
on_contact_added (GObject                  *object,
                  const char               *uid,
                  EContact                 *contact,
                  ContactsComponentFixture *fixture)
{
  fixture->emitter = object;
  fixture->emitted = contact;
}

static void
on_contact_removed (GObject                  *object,
                    const char               *uid,
                    EContact                 *contact,
                    ContactsComponentFixture *fixture)
{
  fixture->emitter = object;
  fixture->emitted = contact;
}

void
add_contact_cb (ValentContactStore       *store,
                GAsyncResult             *result,
                ContactsComponentFixture *fixture)
{
  GError *error = NULL;

  valent_contact_store_add_finish (store, result, &error);
  g_assert_no_error (error);
  g_main_loop_quit (fixture->loop);
}

void
get_contact_cb (ValentContactStore       *store,
                GAsyncResult             *result,
                ContactsComponentFixture *fixture)
{
  GError *error = NULL;

  fixture->result = valent_contact_store_get_contact_finish (store, result, &error);
  g_assert_no_error (error);
  g_main_loop_quit (fixture->loop);
}

void
get_contact_fail_cb (ValentContactStore       *store,
                     GAsyncResult             *result,
                     ContactsComponentFixture *fixture)
{
  g_autoptr (GError) error = NULL;

  fixture->result = valent_contact_store_get_contact_finish (store, result, &error);
  g_assert_error (error, E_CACHE_ERROR, E_CACHE_ERROR_NOT_FOUND);
  g_main_loop_quit (fixture->loop);
}

void
remove_contact_cb (ValentContactStore       *store,
                   GAsyncResult             *result,
                   ContactsComponentFixture *fixture)
{
  GError *error = NULL;

  valent_contact_store_remove_finish (store, result, &error);
  g_assert_no_error (error);
  g_main_loop_quit (fixture->loop);
}

static void
query_contact_cb (ValentContactStore       *store,
                  GAsyncResult             *result,
                  ContactsComponentFixture *fixture)
{
  GError *error = NULL;

  fixture->result = valent_contact_store_query_finish (store, result, &error);
  g_assert_no_error (error);
  g_main_loop_quit (fixture->loop);
}

static void
dup_for_phone_cb (ValentContactStore       *store,
                  GAsyncResult             *result,
                  ContactsComponentFixture *fixture)
{
  GError *error = NULL;

  fixture->result = valent_contact_store_dup_for_phone_finish (store, result, &error);
  g_assert_no_error (error);
  g_main_loop_quit (fixture->loop);
}

static void
contacts_component_fixture_set_up (ContactsComponentFixture *fixture,
                                   gconstpointer             user_data)
{
  g_autoptr (ESource) source = NULL;

  fixture->contacts = valent_contacts_get_default ();

  source = valent_contacts_create_ebook_source ("test-store",
                                                "Test Store",
                                                NULL);
  fixture->store = g_object_new (VALENT_TYPE_CONTACT_STORE,
                                 "source", source,
                                 "name",   "Test Store",
                                 NULL);

  fixture->contact = e_contact_new_from_vcard_with_uid (vcard, "test-contact");
  fixture->loop = g_main_loop_new (NULL, FALSE);

  // HACK: Wait for the provider to be constructed, then a bit longer for
  //       valent_contact_store_provider_load_async() to resolve
  while ((fixture->provider = valent_mock_contact_store_provider_get_instance ()) == NULL)
    continue;

  while (g_main_context_iteration (NULL, FALSE))
    continue;
}

static void
contacts_component_fixture_tear_down (ContactsComponentFixture *fixture,
                                      gconstpointer             user_data)
{
  v_assert_finalize_object (fixture->contacts);
  v_assert_finalize_object (fixture->store);
  g_assert_finalize_object (fixture->contact);
  g_clear_pointer (&fixture->loop, g_main_loop_unref);
}

static void
test_contacts_component_provider (ContactsComponentFixture *fixture,
                                  gconstpointer             user_data)
{
  PeasPluginInfo *info;
  GPtrArray *stores;

  /* Properties */
  g_object_get (fixture->provider,
                "plugin-info", &info,
                NULL);
  g_assert_cmpstr (peas_plugin_info_get_module_name (info), ==, "mock");
  g_boxed_free (PEAS_TYPE_PLUGIN_INFO, info);

  /* Signals */
  g_signal_connect (fixture->provider,
                    "store-added",
                    G_CALLBACK (on_store_added),
                    fixture);
  g_signal_connect (fixture->provider,
                    "store-removed",
                    G_CALLBACK (on_store_removed),
                    fixture);

  /* ::store-added is emitted and the internal representation is updated */
  valent_contact_store_provider_emit_store_added (fixture->provider,
                                                  fixture->store);
  g_assert_true (fixture->emitter == fixture->provider);
  g_assert_true (fixture->emitted == fixture->store);
  fixture->emitter = NULL;
  fixture->emitted = NULL;

  stores = valent_contact_store_provider_get_stores (fixture->provider);
  g_assert_cmpuint (stores->len, ==, 2);
  g_assert_true (g_ptr_array_index (stores, 1) == fixture->store);
  g_clear_pointer (&stores, g_ptr_array_unref);

  /* ::store-removed is emitted and the internal representation is updated */
  valent_contact_store_provider_emit_store_removed (fixture->provider,
                                                    fixture->store);
  g_assert_true (fixture->emitter == fixture->provider);
  g_assert_true (fixture->emitted == fixture->store);
  fixture->emitter = NULL;
  fixture->emitted = NULL;

  stores = valent_contact_store_provider_get_stores (fixture->provider);
  g_assert_cmpuint (stores->len, ==, 1);
  g_clear_pointer (&stores, g_ptr_array_unref);

  g_signal_handlers_disconnect_by_data (fixture->provider, fixture);
}

static void
test_contacts_component_store (ContactsComponentFixture *fixture,
                               gconstpointer             user_data)
{
  g_autofree char *name = NULL;
  g_autofree char *uid = NULL;
  g_autoptr (ESource) source = NULL;
  EBookQuery *query = NULL;
  g_autofree char *sexp = NULL;
  EContact *contact = NULL;
  GSList *contacts = NULL;

  /* Properties */
  g_object_get (fixture->store,
                "name",   &name,
                "uid",    &uid,
                "source", &source,
                NULL);

  g_assert_cmpstr (name, ==, "Test Store");
  g_assert_cmpstr (uid, ==, "test-store");
  g_assert_true (valent_contact_store_get_source (fixture->store) == source);
  g_assert_true (E_IS_SOURCE (source));

  /* Signals */
  g_signal_connect (fixture->store,
                    "contact-added",
                    G_CALLBACK (on_contact_added),
                    fixture);
  g_signal_connect (fixture->store,
                    "contact-removed",
                    G_CALLBACK (on_contact_removed),
                    fixture);

  /* ::contact-added is emitted when contacts are added */
  valent_contact_store_add_contact (fixture->store,
                                    fixture->contact,
                                    NULL,
                                    (GAsyncReadyCallback)add_contact_cb,
                                    fixture);
  g_main_loop_run (fixture->loop);

  g_assert_true (fixture->emitter == fixture->store);
  g_assert_true (fixture->emitted == fixture->contact);
  fixture->emitter = NULL;
  fixture->emitted = NULL;

  /* Contact can be retrieved by UID */
  valent_contact_store_get_contact (fixture->store,
                                    "test-contact",
                                    NULL,
                                    (GAsyncReadyCallback)get_contact_cb,
                                    fixture);
  g_main_loop_run (fixture->loop);

  contact = g_steal_pointer (&fixture->result);
  g_assert_true (E_IS_CONTACT (contact));
  g_assert_cmpstr (e_contact_get_const (contact, E_CONTACT_UID), ==, "test-contact");
  g_clear_object (&contact);

  /* Multiple contacts can be retrieved by UID */
  char *uids[] = {"test-contact", NULL};
  valent_contact_store_get_contacts (fixture->store,
                                     uids,
                                     NULL,
                                     (GAsyncReadyCallback)query_contact_cb,
                                     fixture);
  g_main_loop_run (fixture->loop);

  contacts = g_steal_pointer (&fixture->result);
  g_assert_nonnull (contacts->data);
  contact = g_object_ref (contacts->data);
  g_assert_true (E_IS_CONTACT (contact));
  g_assert_cmpstr (e_contact_get_const (contact, E_CONTACT_UID), ==, "test-contact");
  g_clear_pointer (&contacts, valent_object_slist_free);
  g_clear_object (&contact);

  /* Contacts can be queried with EQuery search expressions */
  query = e_book_query_field_test (E_CONTACT_UID,
                                   E_BOOK_QUERY_IS,
                                   "test-contact");
  sexp = e_book_query_to_string (query);

  valent_contact_store_query (fixture->store,
                              sexp,
                              NULL,
                              (GAsyncReadyCallback)query_contact_cb,
                              fixture);
  g_clear_pointer (&query, e_book_query_unref);
  g_clear_pointer (&sexp, g_free);
  g_main_loop_run (fixture->loop);

  contacts = g_steal_pointer (&fixture->result);
  g_assert_nonnull (contacts->data);
  contact = g_object_ref (contacts->data);
  g_assert_true (E_IS_CONTACT (contact));
  g_assert_cmpstr (e_contact_get_const (contact, E_CONTACT_UID), ==, "test-contact");
  g_clear_pointer (&contacts, valent_object_slist_free);
  g_clear_object (&contact);

  /* Contacts can be queried by telephone number */
  valent_contact_store_dup_for_phone_async (fixture->store,
                                            "+1-123-456-7890",
                                            NULL,
                                            (GAsyncReadyCallback)dup_for_phone_cb,
                                            fixture);
  g_main_loop_run (fixture->loop);

  contact = g_steal_pointer (&fixture->result);
  g_assert_true (E_IS_CONTACT (contact));
  g_assert_cmpstr (e_contact_get_const (contact, E_CONTACT_UID), ==, "test-contact");
  g_clear_object (&contact);

  contact = valent_contact_store_dup_for_phone (fixture->store, "+1-123-456-7890");
  g_assert_true (E_IS_CONTACT (contact));
  g_assert_cmpstr (e_contact_get_const (contact, E_CONTACT_UID), ==, "test-contact");
  g_clear_object (&contact);

  /* ::contact-removed is emitted when contacts are removed */
  valent_contact_store_remove_contact (fixture->store,
                                       "test-contact",
                                       NULL,
                                       (GAsyncReadyCallback)remove_contact_cb,
                                       fixture);
  g_main_loop_run (fixture->loop);

  g_assert_true (fixture->emitter == fixture->store);
  g_assert_true (fixture->emitted == NULL);
  fixture->emitter = NULL;
  fixture->emitted = NULL;

  /* Confirming the contact was removed */
  valent_contact_store_get_contact (fixture->store,
                                    "test-contact",
                                    NULL,
                                    (GAsyncReadyCallback)get_contact_fail_cb,
                                    fixture);
  g_main_loop_run (fixture->loop);

  g_signal_handlers_disconnect_by_data (fixture->store, fixture);
}

static void
test_contacts_component_self (ContactsComponentFixture *fixture,
                              gconstpointer             user_data)
{
  g_autoptr (GPtrArray) stores = NULL;
  ValentContactStore *store;

  g_signal_connect (fixture->contacts,
                    "store-added",
                    G_CALLBACK (on_store_added),
                    fixture);
  g_signal_connect (fixture->contacts,
                    "store-removed",
                    G_CALLBACK (on_store_removed),
                    fixture);

  /* ::store-added propagates to ValentContacts */
  valent_contact_store_provider_emit_store_added (fixture->provider,
                                                  fixture->store);
  g_assert_true (VALENT_IS_CONTACTS (fixture->emitter));
  g_assert_true (VALENT_IS_CONTACT_STORE (fixture->emitted));
  fixture->emitter = NULL;
  fixture->emitted = NULL;

  /* There should be two stores, including the one just added */
  stores = valent_contacts_get_stores (fixture->contacts);
  g_assert_cmpuint (stores->len, ==, 2);

  store = valent_contacts_get_store (fixture->contacts, "test-store");
  g_assert_true (VALENT_IS_CONTACT_STORE (store));

  /* ::store-removed propagates to ValentContacts */
  valent_contact_store_provider_emit_store_removed (fixture->provider,
                                                    fixture->store);
  g_assert_true (VALENT_IS_CONTACTS (fixture->emitter));
  g_assert_true (VALENT_IS_CONTACT_STORE (fixture->emitted));
  fixture->emitter = NULL;
  fixture->emitted = NULL;

  g_signal_handlers_disconnect_by_data (fixture->contacts, fixture);
}

static void
test_contacts_component_utils (void)
{
  g_autoptr (EContact) contact = NULL;
  char *normalized;
  gboolean ret;

  /* Normalize & Compare */
  for (unsigned int i = 0; i < G_N_ELEMENTS (numbers); i++)
    {
      gboolean equal;

      normalized = valent_phone_number_normalize (numbers[i].original);
      g_assert_cmpstr (normalized, ==, numbers[i].normalized);
      g_free (normalized);

      if (i > 0)
        {
          equal = valent_phone_number_equal (numbers[i - 1].original,
                                             numbers[i].original);
          g_assert_true (equal);
        }
    }

  /* Test Contact */
  contact = e_contact_new_from_vcard_with_uid (vcard, "test-contact");
  normalized = valent_phone_number_normalize ("123-456-7890");

  ret = valent_phone_number_of_contact (contact, normalized);
  g_assert_true (ret);
  g_free (normalized);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, G_TEST_OPTION_ISOLATE_DIRS, NULL);

  g_test_add ("/components/contacts/provider",
              ContactsComponentFixture, NULL,
              contacts_component_fixture_set_up,
              test_contacts_component_provider,
              contacts_component_fixture_tear_down);

  g_test_add ("/components/contacts/store",
              ContactsComponentFixture, NULL,
              contacts_component_fixture_set_up,
              test_contacts_component_store,
              contacts_component_fixture_tear_down);

  g_test_add ("/components/contacts/self",
              ContactsComponentFixture, NULL,
              contacts_component_fixture_set_up,
              test_contacts_component_self,
              contacts_component_fixture_tear_down);

  g_test_add_func ("/components/contacts/utils",
                   test_contacts_component_utils);

  return g_test_run ();
}
