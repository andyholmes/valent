// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-mock-contacts-adapter"

#include "config.h"

#include <gio/gio.h>
#include <libvalent-contacts.h>

#include "valent-mock-contacts-adapter.h"


/**
 * ValentMockContactsAdapter:
 *
 * #ValentMockContactsAdapter is a mock contact store adapter for testing
 * purposes. It loads with a single contact store, with a single contact.
 *
 * The store UID and name are `mock-store' and `Mock Store`, respectively. The
 * contact UID and name are `mock-contact` and `Mock Contact`, respectively,
 * with a telephone number of `123-456-7890`.
 *
 * Simply call valent_contacts_adapter_store_added() to add more
 * stores and valent_contacts_adapter_store_removed() to remove them.
 */

struct _ValentMockContactsAdapter
{
  ValentContactsAdapter  parent_instance;
};

G_DEFINE_TYPE (ValentMockContactsAdapter, valent_mock_contacts_adapter, VALENT_TYPE_CONTACTS_ADAPTER)


/*
 * ValentContactsAdapter
 */
static void
valent_mock_contacts_adapter_load_async (ValentContactsAdapter *adapter,
                                         GCancellable          *cancellable,
                                         GAsyncReadyCallback    callback,
                                         gpointer               user_data)
{
  g_autoptr (GTask) task = NULL;
  g_autoptr (ESource) source = NULL;
  g_autoptr (EContact) contact = NULL;
  g_autoptr (ValentContactStore) store = NULL;

  g_assert (VALENT_IS_MOCK_CONTACTS_ADAPTER (adapter));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  /* Mock Store */
  source = e_source_new_with_uid ("mock-store", NULL, NULL);
  e_source_set_display_name (source, "Mock Store");

  store = g_object_new (VALENT_TYPE_CONTACT_CACHE,
                        "source", source,
                        NULL);
  valent_contacts_adapter_store_added (adapter, store);

  /* Mock Contact */
  contact = e_contact_new_from_vcard_with_uid ("BEGIN:VCARD\n"
                                               "VERSION:2.1\n"
                                               "FN:Mock Contact\n"
                                               "TEL;CELL:123-456-7890\n"
                                               "END:VCARD\n",
                                               "mock-contact");
  valent_contact_store_add_contact (store, contact, NULL, NULL, NULL);

  /* Token Source */
  task = g_task_new (adapter, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_mock_contacts_adapter_load_async);
  g_task_return_boolean (task, TRUE);
}

/*
 * GObject
 */
static void
valent_mock_contacts_adapter_class_init (ValentMockContactsAdapterClass *klass)
{
  ValentContactsAdapterClass *adapter_class = VALENT_CONTACTS_ADAPTER_CLASS (klass);

  adapter_class->load_async = valent_mock_contacts_adapter_load_async;
}

static void
valent_mock_contacts_adapter_init (ValentMockContactsAdapter *self)
{
}

