// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-mock-contact-store-provider"

#include "config.h"

#include <gio/gio.h>
#include <libvalent-contacts.h>

#include "valent-mock-contact-store-provider.h"


/**
 * SECTION:valentmockcontactstoreprovider
 * @short_description: Mock contact store provider
 * @title: ValentMockContactStoreProvider
 * @stability: Unstable
 * @include: libvalent-test.h
 *
 * #ValentMockContactStoreProvider is a mock contact store provider for testing
 * purposes. It loads with a single contact store, with a single contact.
 *
 * The store UID and name are `mock-store' and `Mock Store`, respectively. The
 * contact UID and name are `mock-contact` and `Mock Contact`, respectively,
 * with a telephone number of `123-456-7890`.
 *
 * Simply call valent_contact_store_provider_emit_store_added() to add more
 * stores and valent_contact_store_provider_emit_store_removed() to remove them.
 */

struct _ValentMockContactStoreProvider
{
  ValentContactStoreProvider  parent_instance;
};

G_DEFINE_TYPE (ValentMockContactStoreProvider, valent_mock_contact_store_provider, VALENT_TYPE_CONTACT_STORE_PROVIDER)


static ValentContactStoreProvider *test_instance = NULL;

/*
 * ValentContactStoreProvider
 */
static void
valent_mock_contact_store_provider_load_async (ValentContactStoreProvider *provider,
                                               GCancellable               *cancellable,
                                               GAsyncReadyCallback         callback,
                                               gpointer                    user_data)
{
  g_autoptr (GTask) task = NULL;
  g_autoptr (ESource) source = NULL;
  g_autoptr (EContact) contact = NULL;
  g_autoptr (ValentContactStore) store = NULL;

  g_assert (VALENT_IS_MOCK_CONTACT_STORE_PROVIDER (provider));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  /* Mock Store */
  source = e_source_new_with_uid ("mock-store", NULL, NULL);
  e_source_set_display_name (source, "Mock Store");

  store = g_object_new (VALENT_TYPE_CONTACT_CACHE,
                        "source", source,
                        NULL);
  valent_contact_store_provider_emit_store_added (provider, store);

  /* Mock Contact */
  contact = e_contact_new_from_vcard_with_uid ("BEGIN:VCARD\n"
                                               "VERSION:2.1\n"
                                               "FN:Mock Contact\n"
                                               "TEL;CELL:123-456-7890\n"
                                               "END:VCARD\n",
                                               "mock-contact");
  valent_contact_store_add_contact (store, contact, NULL, NULL, NULL);

  /* Token Source */
  task = g_task_new (provider, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_mock_contact_store_provider_load_async);
  g_task_return_boolean (task, TRUE);
}

/*
 * GObject
 */
static void
valent_mock_contact_store_provider_class_init (ValentMockContactStoreProviderClass *klass)
{
  ValentContactStoreProviderClass *provider_class = VALENT_CONTACT_STORE_PROVIDER_CLASS (klass);

  provider_class->load_async = valent_mock_contact_store_provider_load_async;
}

static void
valent_mock_contact_store_provider_init (ValentMockContactStoreProvider *self)
{
  if (test_instance == NULL)
    {
      test_instance = VALENT_CONTACT_STORE_PROVIDER (self);
      g_object_add_weak_pointer (G_OBJECT (test_instance),
                                 (gpointer)&test_instance);
    }
}

/**
 * valent_mock_contact_store_provider_get_instance:
 *
 * Get the #ValentMockContactStoreProvider instance.
 *
 * Returns: (transfer none) (nullable): a #ValentContactStoreProvider
 */
ValentContactStoreProvider *
valent_mock_contact_store_provider_get_instance (void)
{
  return test_instance;
}

