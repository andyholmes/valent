// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-mock-contacts-adapter"

#include "config.h"

#include <gio/gio.h>
#include <libvalent-contacts.h>

#include "valent-contact-cache-private.h"
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

static void   g_async_initable_iface_init (GAsyncInitableIface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (ValentMockContactsAdapter, valent_mock_contacts_adapter, VALENT_TYPE_CONTACTS_ADAPTER,
                               G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE, g_async_initable_iface_init))


/*
 * GAsyncInitable
 */
static void
valent_contact_store_add_contact_cb (ValentContactStore *store,
                                     GAsyncResult       *result,
                                     gboolean           *done)
{
  g_autoptr (GError) error = NULL;

  if (!valent_contact_store_add_contacts_finish (store, result, &error))
    g_assert_no_error (error);

  if (done != NULL)
    *done = TRUE;
}

static void
valent_mock_contacts_adapter_init_async (GAsyncInitable      *initable,
                                         int                  io_priority,
                                         GCancellable        *cancellable,
                                         GAsyncReadyCallback  callback,
                                         gpointer             user_data)
{
  g_autoptr (GTask) task = NULL;
  g_autoptr (GCancellable) destroy = NULL;
  g_autoptr (ESource) source = NULL;
  g_autoptr (EContact) contact = NULL;
  g_autoptr (ValentContactStore) store = NULL;
  gboolean done = FALSE;

  g_assert (VALENT_IS_MOCK_CONTACTS_ADAPTER (initable));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  /* Cancel initialization if the object is destroyed */
  destroy = valent_object_attach_cancellable (VALENT_OBJECT (initable),
                                              cancellable);

  task = g_task_new (initable, destroy, callback, user_data);
  g_task_set_priority (task, io_priority);
  g_task_set_source_tag (task, valent_mock_contacts_adapter_init_async);

  /* Mock Store */
  source = e_source_new_with_uid ("mock-store", NULL, NULL);
  e_source_set_display_name (source, "Mock Store");

  store = g_object_new (VALENT_TYPE_CONTACT_CACHE,
                        "source", source,
                        NULL);

  /* Mock Contact */
  contact = e_contact_new_from_vcard_with_uid ("BEGIN:VCARD\n"
                                               "VERSION:2.1\n"
                                               "FN:Mock Contact\n"
                                               "TEL;CELL:123-456-7890\n"
                                               "END:VCARD\n",
                                               "mock-contact");

  /* Add the contact to the store, then add the store to the adapter */
  valent_contact_store_add_contact (store,
                                    contact,
                                    destroy,
                                    (GAsyncReadyCallback)valent_contact_store_add_contact_cb,
                                    &done);

  while (!done)
    g_main_context_iteration (NULL, FALSE);

  valent_contacts_adapter_store_added (g_task_get_source_object (task), store);
  g_task_return_boolean (task, TRUE);
}

static void
g_async_initable_iface_init (GAsyncInitableIface *iface)
{
  iface->init_async = valent_mock_contacts_adapter_init_async;
}

/*
 * GObject
 */
static void
valent_mock_contacts_adapter_class_init (ValentMockContactsAdapterClass *klass)
{
}

static void
valent_mock_contacts_adapter_init (ValentMockContactsAdapter *self)
{
}

