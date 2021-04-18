// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-mock-contact-store-provider"

#include "config.h"

#include <gio/gio.h>
#include <libvalent-contacts.h>

#include "valent-mock-contact-store-provider.h"


struct _ValentMockContactStoreProvider
{
  ValentContactStoreProvider  parent_instance;
};

G_DEFINE_TYPE (ValentMockContactStoreProvider, valent_mock_contact_store_provider, VALENT_TYPE_CONTACT_STORE_PROVIDER)


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

  g_assert (VALENT_IS_MOCK_CONTACT_STORE_PROVIDER (provider));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

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
}

