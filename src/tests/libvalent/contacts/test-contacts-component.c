// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>
#include <libvalent-core.h>
#include <libvalent-contacts.h>
#include <libvalent-test.h>


typedef struct
{
  ValentContacts     *contacts;
  ValentContactStore *store;
  EContact           *contact;
  GMainLoop          *loop;
  gpointer            data;
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
  fixture->data = object;
}

static void
on_store_removed (GObject                  *object,
                  ValentContactStore       *store,
                  ContactsComponentFixture *fixture)
{
  fixture->data = object;
}

static void
on_contact_added (GObject                  *object,
                  const char               *uid,
                  EContact                 *contact,
                  ContactsComponentFixture *fixture)
{
  fixture->data = object;
}

static void
on_contact_removed (GObject                  *object,
                    const char               *uid,
                    EContact                 *contact,
                    ContactsComponentFixture *fixture)
{
  fixture->data = object;
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

  fixture->data = valent_contact_store_get_contact_finish (store, result, &error);
  g_assert_no_error (error);
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

  fixture->data = valent_contact_store_query_finish (store, result, &error);
  g_assert_no_error (error);
  g_main_loop_quit (fixture->loop);
}

static void
dup_for_phone_cb (ValentContactStore       *store,
                  GAsyncResult             *result,
                  ContactsComponentFixture *fixture)
{
  GError *error = NULL;

  fixture->data = valent_contact_store_dup_for_phone_finish (store, result, &error);
  g_assert_no_error (error);
  g_main_loop_quit (fixture->loop);
}

static void
contacts_component_fixture_set_up (ContactsComponentFixture *fixture,
                                   gconstpointer             user_data)
{
  g_autoptr (ESource) source = NULL;

  fixture->contacts = valent_contacts_get_default ();

  source = valent_contacts_create_ebook_source ("test-store", "Test Store", NULL);
  fixture->store = g_object_new (VALENT_TYPE_CONTACT_STORE,
                                 "source", source,
                                 NULL);

  fixture->contact = e_contact_new_from_vcard_with_uid (vcard, "test-contact");
  fixture->loop = g_main_loop_new (NULL, FALSE);
}

static void
contacts_component_fixture_tear_down (ContactsComponentFixture *fixture,
                                      gconstpointer             user_data)
{
  g_assert_finalize_object (fixture->contacts);
  g_assert_finalize_object (fixture->store);
  g_assert_finalize_object (fixture->contact);
  g_clear_pointer (&fixture->loop, g_main_loop_unref);
}

static void
test_contacts_component_provider (ContactsComponentFixture *fixture,
                               gconstpointer          user_data)
{
  g_autoptr (GPtrArray) extensions = NULL;
  ValentContactStoreProvider *provider;
  PeasPluginInfo *info;

  /* Wait for valent_contact_store_provider_load_async() to resolve */
  extensions = valent_component_get_extensions (VALENT_COMPONENT (fixture->contacts));
  provider = g_ptr_array_index (extensions, 0);

  while (g_main_context_iteration (NULL, FALSE))
    continue;

  /* Properties */
  g_object_get (provider,
                "plugin-info", &info,
                NULL);
  g_assert_nonnull (info);
  g_boxed_free (PEAS_TYPE_PLUGIN_INFO, info);

  /* Signals */
  g_signal_connect (provider,
                    "store-added",
                    G_CALLBACK (on_store_added),
                    fixture);

  valent_contact_store_provider_emit_store_added (provider, fixture->store);
  g_assert_true (fixture->data == provider);
  fixture->data = NULL;

  g_signal_connect (provider,
                    "store-removed",
                    G_CALLBACK (on_store_removed),
                    fixture);

  valent_contact_store_provider_emit_store_removed (provider, fixture->store);
  g_assert_true (fixture->data == provider);
  fixture->data = NULL;
}

static void
test_contacts_component_store (ContactsComponentFixture *fixture,
                               gconstpointer             user_data)
{
  g_autoptr (GPtrArray) extensions = NULL;
  ValentContactStoreProvider *provider;

  g_autofree char *name = NULL;
  g_autofree char *uid = NULL;
  g_autoptr (ESource) source = NULL;

  EBookQuery *query = NULL;
  g_autofree char *sexp = NULL;
  EContact *contact = NULL;
  GSList *contacts = NULL;

  /* Wait for valent_contact_store_provider_load_async() to resolve */
  extensions = valent_component_get_extensions (VALENT_COMPONENT (fixture->contacts));
  provider = g_ptr_array_index (extensions, 0);

  while (g_main_context_iteration (NULL, FALSE))
    continue;

  /* Add Store */
  g_signal_connect (provider,
                    "store-added",
                    G_CALLBACK (on_store_added),
                    fixture);

  valent_contact_store_provider_emit_store_added (provider, fixture->store);
  g_assert_true (fixture->data == provider);
  fixture->data = NULL;

  /* Check properties */
  g_object_get (fixture->store,
                "name",   &name,
                "uid",    &uid,
                "source", &source,
                NULL);

  g_assert_cmpstr (name, ==, "Test Store");
  g_assert_cmpstr (uid, ==, "test-store");
  g_assert_nonnull (source);

  /* Add contact */
  g_signal_connect (fixture->store,
                    "contact-added",
                    G_CALLBACK (on_contact_added),
                    fixture);

  valent_contact_store_add_contact (fixture->store,
                                    fixture->contact,
                                    NULL,
                                    (GAsyncReadyCallback)add_contact_cb,
                                    fixture);
  g_main_loop_run (fixture->loop);

  g_assert_true (fixture->data == fixture->store);
  fixture->data = NULL;

  /* Get single contact */
  valent_contact_store_get_contact (fixture->store,
                                    "test-contact",
                                    NULL,
                                    (GAsyncReadyCallback)get_contact_cb,
                                    fixture);
  g_main_loop_run (fixture->loop);

  contact = g_steal_pointer (&fixture->data);
  g_assert_cmpstr (e_contact_get_const (contact, E_CONTACT_UID), ==, "test-contact");
  g_clear_object (&contact);
  fixture->data = NULL;

  /* Get multiple contacts */
  char *uids[] = {"test-contact", NULL};
  valent_contact_store_get_contacts (fixture->store,
                                     uids,
                                     NULL,
                                     (GAsyncReadyCallback)query_contact_cb,
                                     fixture);
  g_main_loop_run (fixture->loop);

  contacts = g_steal_pointer (&fixture->data);
  g_assert_cmpstr (e_contact_get_const (contacts->data, E_CONTACT_UID), ==, "test-contact");
  g_clear_pointer (&contacts, valent_object_slist_free);
  fixture->data = NULL;

  /* Query contacts */
  query = e_book_query_field_test (E_CONTACT_UID, E_BOOK_QUERY_IS, "test-contact");
  sexp = e_book_query_to_string (query);

  valent_contact_store_query (fixture->store,
                              sexp,
                              NULL,
                              (GAsyncReadyCallback)query_contact_cb,
                              fixture);
  g_main_loop_run (fixture->loop);

  contacts = g_steal_pointer (&fixture->data);
  g_assert_cmpstr (e_contact_get_const (contacts->data, E_CONTACT_UID), ==, "test-contact");
  g_clear_pointer (&query, e_book_query_unref);
  g_clear_pointer (&sexp, g_free);
  g_clear_pointer (&contacts, valent_object_slist_free);
  fixture->data = NULL;

  /* Query phone number */
  valent_contact_store_dup_for_phone_async (fixture->store,
                                            "+1-123-456-7890",
                                            NULL,
                                            (GAsyncReadyCallback)dup_for_phone_cb,
                                            fixture);
  g_main_loop_run (fixture->loop);

  contact = g_steal_pointer (&fixture->data);
  g_assert_cmpstr (e_contact_get_const (contact, E_CONTACT_UID), ==, "test-contact");
  g_clear_object (&contact);
  fixture->data = NULL;

  contact = valent_contact_store_dup_for_phone (fixture->store, "+1-123-456-7890");
  g_assert_cmpstr (e_contact_get_const (contact, E_CONTACT_UID), ==, "test-contact");
  g_clear_object (&contact);

  /* Remove contact */
  g_signal_connect (fixture->store,
                    "contact-removed",
                    G_CALLBACK (on_contact_removed),
                    fixture);

  valent_contact_store_remove_contact (fixture->store,
                                       e_contact_get_const (fixture->contact, E_CONTACT_UID),
                                       NULL,
                                       (GAsyncReadyCallback)remove_contact_cb,
                                       fixture);
  g_main_loop_run (fixture->loop);

  g_assert_true (fixture->data == fixture->store);
  fixture->data = NULL;

  /* Remove Store */
  g_signal_connect (provider,
                    "store-removed",
                    G_CALLBACK (on_store_removed),
                    fixture);

  valent_contact_store_provider_emit_store_removed (provider, fixture->store);
  g_assert_true (fixture->data == provider);
  fixture->data = NULL;
}

static void
test_contacts_component_self (ContactsComponentFixture *fixture,
                              gconstpointer             user_data)
{
  g_autoptr (GPtrArray) extensions = NULL;
  ValentContactStoreProvider *provider;

  /* Wait for valent_contact_store_provider_load_async() to resolve */
  extensions = valent_component_get_extensions (VALENT_COMPONENT (fixture->contacts));
  provider = g_ptr_array_index (extensions, 0);

  while (g_main_context_iteration (NULL, FALSE))
    continue;

  /* Add Store */
  g_signal_connect (fixture->contacts,
                    "store-added",
                    G_CALLBACK (on_store_added),
                    fixture);

  valent_contact_store_provider_emit_store_added (provider, fixture->store);
  g_assert_true (fixture->data == fixture->contacts);
  fixture->data = NULL;

  /* Change Store */

  /* Remove Store */
  g_signal_connect (fixture->contacts,
                    "store-removed",
                    G_CALLBACK (on_store_removed),
                    fixture);

  valent_contact_store_provider_emit_store_removed (provider, fixture->store);
  g_assert_true (fixture->data == fixture->contacts);
  fixture->data = NULL;
}

static void
test_contacts_component_dispose (ContactsComponentFixture *fixture,
                                 gconstpointer             user_data)
{
  GPtrArray *extensions;
  ValentContactStoreProvider *provider;
  PeasEngine *engine;
  g_autoptr (GSettings) settings = NULL;

  /* Add a store to the provider */
  extensions = valent_component_get_extensions (VALENT_COMPONENT (fixture->contacts));
  provider = g_ptr_array_index (extensions, 0);
  g_ptr_array_unref (extensions);

  /* Wait for provider to resolve */
  valent_contact_store_provider_emit_store_added (provider, fixture->store);

  while (g_main_context_iteration (NULL, FALSE))
    continue;

  /* Disable/Enable the provider */
  settings = valent_component_new_settings ("contacts", "mock");

  g_settings_set_boolean (settings, "enabled", FALSE);
  extensions = valent_component_get_extensions (VALENT_COMPONENT (fixture->contacts));
  g_assert_cmpuint (extensions->len, ==, 0);
  g_ptr_array_unref (extensions);

  g_settings_set_boolean (settings, "enabled", TRUE);
  extensions = valent_component_get_extensions (VALENT_COMPONENT (fixture->contacts));
  g_assert_cmpuint (extensions->len, ==, 1);
  g_ptr_array_unref (extensions);

  /* Unload the provider */
  engine = valent_get_engine ();
  peas_engine_unload_plugin (engine, peas_engine_get_plugin_info (engine, "mock"));

  extensions = valent_component_get_extensions (VALENT_COMPONENT (fixture->contacts));
  g_assert_cmpuint (extensions->len, ==, 0);
  g_ptr_array_unref (extensions);
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
          equal = valent_phone_number_equal (numbers[i - 1].original, numbers[i].original);
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

  g_test_add ("/components/contacts/dispose",
              ContactsComponentFixture, NULL,
              contacts_component_fixture_set_up,
              test_contacts_component_dispose,
              contacts_component_fixture_tear_down);

  g_test_add_func ("/components/contacts/utils",
                   test_contacts_component_utils);

  return g_test_run ();
}
