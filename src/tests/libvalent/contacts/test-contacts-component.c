// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>
#include <libvalent-core.h>
#include <libvalent-contacts.h>
#include <libvalent-test.h>


typedef struct
{
  ValentContacts        *contacts;
  ValentContactsAdapter *adapter;
  ValentContactStore    *store;
  EContact              *contact;
  GMainLoop             *loop;
  gpointer               emitter;
  gpointer               emitted;
  gpointer               result;
} ContactsComponentFixture;


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
                  EContact                 *contact,
                  ContactsComponentFixture *fixture)
{
  fixture->emitter = object;
  fixture->emitted = contact;
}

static void
on_contact_removed (GObject                  *object,
                    const char               *uid,
                    ContactsComponentFixture *fixture)
{
  fixture->emitter = object;
  fixture->emitted = g_strdup (uid);
}

void
add_contact_cb (ValentContactStore       *store,
                GAsyncResult             *result,
                ContactsComponentFixture *fixture)
{
  GError *error = NULL;

  valent_contact_store_add_contacts_finish (store, result, &error);
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

  valent_contact_store_remove_contacts_finish (store, result, &error);
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
contacts_component_fixture_set_up (ContactsComponentFixture *fixture,
                                   gconstpointer             user_data)
{
  g_autoptr (ESource) source = NULL;

  fixture->contacts = valent_contacts_get_default ();
  fixture->adapter = valent_test_await_adapter (fixture->contacts);

  /* Create a test store */
  source = e_source_new_with_uid ("test-store", NULL, NULL);
  e_source_set_display_name (source, "Test Store");
  fixture->store = g_object_new (VALENT_TYPE_CONTACT_CACHE,
                                 "source", source,
                                 "name",   "Test Store",
                                 NULL);

  fixture->contact = e_contact_new_from_vcard_with_uid (vcard, "test-contact");
  fixture->loop = g_main_loop_new (NULL, FALSE);

  g_object_ref (fixture->adapter);
}

static void
contacts_component_fixture_tear_down (ContactsComponentFixture *fixture,
                                      gconstpointer             user_data)
{
  v_await_finalize_object (fixture->contacts);
  v_await_finalize_object (fixture->adapter);
  v_await_finalize_object (fixture->store);
  v_assert_finalize_object (fixture->contact);
  g_clear_pointer (&fixture->loop, g_main_loop_unref);
}

static void
test_contacts_component_adapter (ContactsComponentFixture *fixture,
                                 gconstpointer             user_data)
{
  PeasPluginInfo *info;
  GPtrArray *stores;

  /* Properties */
  g_object_get (fixture->adapter,
                "plugin-info", &info,
                NULL);
  g_assert_cmpstr (peas_plugin_info_get_module_name (info), ==, "mock");
  g_boxed_free (PEAS_TYPE_PLUGIN_INFO, info);

  /* Signals */
  g_signal_connect (fixture->adapter,
                    "store-added",
                    G_CALLBACK (on_store_added),
                    fixture);
  g_signal_connect (fixture->adapter,
                    "store-removed",
                    G_CALLBACK (on_store_removed),
                    fixture);

  /* ::store-added is emitted and the internal representation is updated */
  valent_contacts_adapter_emit_store_added (fixture->adapter, fixture->store);
  g_assert_true (fixture->emitter == fixture->adapter);
  g_assert_true (fixture->emitted == fixture->store);
  fixture->emitter = NULL;
  fixture->emitted = NULL;

  stores = valent_contacts_adapter_get_stores (fixture->adapter);
  g_assert_cmpuint (stores->len, ==, 2);
  g_assert_true (g_ptr_array_index (stores, 1) == fixture->store);
  g_clear_pointer (&stores, g_ptr_array_unref);

  /* ::store-removed is emitted and the internal representation is updated */
  valent_contacts_adapter_emit_store_removed (fixture->adapter, fixture->store);
  g_assert_true (fixture->emitter == fixture->adapter);
  g_assert_true (fixture->emitted == fixture->store);
  fixture->emitter = NULL;
  fixture->emitted = NULL;

  stores = valent_contacts_adapter_get_stores (fixture->adapter);
  g_assert_cmpuint (stores->len, ==, 1);
  g_clear_pointer (&stores, g_ptr_array_unref);

  g_signal_handlers_disconnect_by_data (fixture->adapter, fixture);
}

static void
test_contacts_component_store (ContactsComponentFixture *fixture,
                               gconstpointer             user_data)
{
  g_autofree char *name = NULL;
  g_autofree char *path = NULL;
  g_autofree char *uid = NULL;
  g_autoptr (ESource) source = NULL;
  EBookQuery *query = NULL;
  g_autofree char *sexp = NULL;
  EContact *contact = NULL;
  GSList *contacts = NULL;

  /* Properties */
  g_object_get (fixture->store,
                "name",   &name,
                "path",   &path,
                "uid",    &uid,
                "source", &source,
                NULL);

  g_assert_cmpstr (name, ==, "Test Store");
  g_assert_nonnull (path);
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

  /* ::contact-removed is emitted when contacts are removed */
  valent_contact_store_remove_contact (fixture->store,
                                       "test-contact",
                                       NULL,
                                       (GAsyncReadyCallback)remove_contact_cb,
                                       fixture);
  g_main_loop_run (fixture->loop);

  g_assert_true (fixture->emitter == fixture->store);
  g_assert_cmpstr (fixture->emitted, ==, "test-contact");
  fixture->emitter = NULL;
  g_clear_pointer (&fixture->emitted, g_free);

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
  valent_contacts_adapter_emit_store_added (fixture->adapter, fixture->store);
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
  valent_contacts_adapter_emit_store_removed (fixture->adapter, fixture->store);
  g_assert_true (VALENT_IS_CONTACTS (fixture->emitter));
  g_assert_true (VALENT_IS_CONTACT_STORE (fixture->emitted));
  fixture->emitter = NULL;
  fixture->emitted = NULL;

  g_signal_handlers_disconnect_by_data (fixture->contacts, fixture);
}

int
main (int   argc,
      char *argv[])
{
  valent_test_init (&argc, &argv, NULL);

  g_test_add ("/components/contacts/adapter",
              ContactsComponentFixture, NULL,
              contacts_component_fixture_set_up,
              test_contacts_component_adapter,
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

  return g_test_run ();
}
