// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-contacts"

#include "config.h"

#include <gio/gio.h>
#include <libvalent-core.h>

#include "valent-contacts.h"
#include "valent-contact-store.h"
#include "valent-contact-store-provider.h"
#include "valent-contact-utils.h"


/**
 * SECTION:valent-contacts
 * @short_description: Contacts Abstraction
 * @title: ValentContacts
 * @stability: Unstable
 * @include: libvalent-contacts.h
 *
 * #ValentContacts is an abstraction of desktop contact sources, with a simple API
 * generally intended to be used by #ValentDevicePlugin implementations.
 *
 * ## Providers and Models ##
 *
 * #ValentContacts loads and tracks implementations of #ValentContactStoreProvider known
 * to the default #ValentEngine. Each #ValentContactStoreProvider may provide stores
 * implementing #ValentContactStore and create new stores backed by that provider.
 *
 * For example, the Evolution Data Server (`eds`) plugin implements
 * #ValentContactStoreProvider in #ValentEBookProvider and provides a #ValentEBookStore for
 * each address book known to Evolution. If no other provider is available,
 * #ValentContacts will fallback to providing #ValentContactStore objects, a simple
 * #EBookCache implementation of #ValentContactStore.
 */

struct _ValentContacts
{
  ValentComponent             parent_instance;

  GCancellable               *cancellable;

  ValentContactStoreProvider *default_provider;
  GPtrArray                  *stores;
};

G_DEFINE_TYPE (ValentContacts, valent_contacts, VALENT_TYPE_COMPONENT)

enum
{
  STORE_ADDED,
  STORE_REMOVED,
  N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0, };


static ValentContacts *default_contacts = NULL;


/*
 * ValentContactStoreProvider Callbacks
 */
static void
on_store_added (ValentContactStoreProvider *provider,
                ValentContactStore         *store,
                ValentContacts             *self)
{
  VALENT_ENTRY;

  g_assert (VALENT_IS_CONTACTS (self));

  VALENT_TRACE_MSG ("%s: %s", G_OBJECT_TYPE_NAME (store), valent_contact_store_get_name (store));

  g_ptr_array_add (self->stores, g_object_ref (store));
  g_signal_emit (G_OBJECT (self), signals [STORE_ADDED], 0, store);

  VALENT_EXIT;
}

static void
on_store_removed (ValentContactStoreProvider *provider,
                  ValentContactStore         *store,
                  ValentContacts             *self)
{
  VALENT_ENTRY;

  g_assert (VALENT_IS_CONTACTS (self));

  VALENT_TRACE_MSG ("%s: %s", G_OBJECT_TYPE_NAME (store), valent_contact_store_get_name (store));

  g_ptr_array_remove (self->stores, store);
  g_signal_emit (G_OBJECT (self), signals [STORE_REMOVED], 0, store);

  VALENT_EXIT;
}

static void
valent_contact_store_provider_load_cb (ValentContactStoreProvider *provider,
                                       GAsyncResult               *result,
                                       ValentContacts             *contacts)
{
  g_autoptr (GError) error = NULL;

  VALENT_ENTRY;

  if (!valent_contact_store_provider_load_finish (provider, result, &error) &&
      !valent_error_ignore (error))
    g_warning ("%s: %s", G_OBJECT_TYPE_NAME (provider), error->message);

  VALENT_EXIT;
}


/*
 * ValentComponent
 */
static void
valent_contacts_provider_added (ValentComponent *component,
                                PeasExtension   *extension)
{
  ValentContacts *self = VALENT_CONTACTS (component);
  ValentContactStoreProvider *provider = VALENT_CONTACT_STORE_PROVIDER (extension);

  g_assert (VALENT_IS_CONTACTS (self));
  g_assert (VALENT_IS_CONTACT_STORE_PROVIDER (provider));

  if (self->default_provider == NULL)
    g_set_object (&self->default_provider, provider);

  g_signal_connect_object (provider,
                           "store-added",
                           G_CALLBACK (on_store_added),
                           self,
                           0);

  g_signal_connect_object (provider,
                           "store-removed",
                           G_CALLBACK (on_store_removed),
                           self,
                           0);

  valent_contact_store_provider_load_async (provider,
                                            self->cancellable,
                                            (GAsyncReadyCallback)valent_contact_store_provider_load_cb,
                                            self);
}

static void
valent_contacts_provider_removed (ValentComponent *component,
                                  PeasExtension   *extension)
{
  ValentContacts *self = VALENT_CONTACTS (component);
  ValentContactStoreProvider *provider = VALENT_CONTACT_STORE_PROVIDER (extension);
  g_autoptr (GPtrArray) stores = NULL;

  g_assert (VALENT_IS_CONTACTS (self));
  g_assert (VALENT_IS_CONTACT_STORE_PROVIDER (provider));

  if (self->default_provider == provider)
    g_clear_object (&self->default_provider);

  /* Simulate removal */
  stores = valent_contact_store_provider_get_stores (provider);

  for (unsigned int i = 0; i < stores->len; i++)
    valent_contact_store_provider_emit_store_removed (provider, g_ptr_array_index (stores, i));

  g_signal_handlers_disconnect_by_func (provider, on_store_added, self);
  g_signal_handlers_disconnect_by_func (provider, on_store_removed, self);
}

/*
 * GObject
 */
static void
valent_contacts_dispose (GObject *object)
{
  ValentContacts *self = VALENT_CONTACTS (object);

  if (!g_cancellable_is_cancelled (self->cancellable))
    g_cancellable_cancel (self->cancellable);

  G_OBJECT_CLASS (valent_contacts_parent_class)->dispose (object);
}

static void
valent_contacts_finalize (GObject *object)
{
  ValentContacts *self = VALENT_CONTACTS (object);

  g_clear_object (&self->cancellable);
  g_clear_pointer (&self->stores, g_ptr_array_unref);
  g_clear_object (&self->default_provider);

  G_OBJECT_CLASS (valent_contacts_parent_class)->finalize (object);
}

static void
valent_contacts_class_init (ValentContactsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentComponentClass *component_class = VALENT_COMPONENT_CLASS (klass);

  /* Ensure we don't hit a MT race condition */
  g_type_ensure (E_TYPE_SOURCE);

  object_class->dispose = valent_contacts_dispose;
  object_class->finalize = valent_contacts_finalize;

  component_class->provider_added = valent_contacts_provider_added;
  component_class->provider_removed = valent_contacts_provider_removed;

  /**
   * ValentContacts::store-added:
   * @contacts: a #ValentContacts
   * @store: a #ValentContactStore
   *
   * ValentContactStore::store-added is emitted when a contact store (address
   * book) is added to @store.
   */
  signals [STORE_ADDED] =
    g_signal_new ("store-added",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, VALENT_TYPE_CONTACT_STORE);
  g_signal_set_va_marshaller (signals [STORE_ADDED],
                              G_TYPE_FROM_CLASS (klass),
                              g_cclosure_marshal_VOID__OBJECTv);

  /**
   * ValentContacts::store-removed:
   * @contacts: a #ValentContacts
   * @store: a #ValentContactStore
   *
   * ValentContactStore::store-removed is emitted when a contact store (address
   * book) is removed from @store.
   */
  signals [STORE_REMOVED] =
    g_signal_new ("store-removed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, VALENT_TYPE_CONTACT_STORE);
  g_signal_set_va_marshaller (signals [STORE_ADDED],
                              G_TYPE_FROM_CLASS (klass),
                              g_cclosure_marshal_VOID__OBJECTv);
}

static void
valent_contacts_init (ValentContacts *self)
{
  self->cancellable = g_cancellable_new ();
  self->stores = g_ptr_array_new_with_free_func (g_object_unref);
}

/**
 * valent_contacts_get_default:
 *
 * Get the default #ValentContacts.
 *
 * Returns: (transfer none): The default contacts
 */
ValentContacts *
valent_contacts_get_default (void)
{
  if (default_contacts == NULL)
    {
      default_contacts = g_object_new (VALENT_TYPE_CONTACTS,
                                       "plugin-context", "contacts",
                                       "plugin-type",    VALENT_TYPE_CONTACT_STORE_PROVIDER,
                                       NULL);

      g_object_add_weak_pointer (G_OBJECT (default_contacts), (gpointer) &default_contacts);
    }

  return default_contacts;
}

/**
 * valent_contacts_ensure_store:
 * @contacts: (nullable): a #ValentContacts
 * @uid: a unique id
 * @name: a display name
 *
 * Get a #ValentContactStore for @uid. If the contact store does not exist, one
 * will be created using the default provider and passed @name and @description.
 *
 * Returns: (transfer none): a #ValentContactStore
 */
ValentContactStore *
valent_contacts_ensure_store (ValentContacts *contacts,
                              const char     *uid,
                              const char     *name)
{
  ValentContactStore *store;
  g_autoptr (ESource) source = NULL;

  g_return_val_if_fail (uid != NULL, NULL);
  g_return_val_if_fail (name != NULL, NULL);

  /* Use the default ValentContacts */
  if (contacts == NULL)
    contacts = valent_contacts_get_default ();

  /* Try to find an existing store */
  if ((store = valent_contacts_get_store (contacts, uid)) != NULL)
    return store;

  /* Create a new store */
  source = valent_contacts_create_ebook_source (uid, name, NULL);
  store = g_object_new (VALENT_TYPE_CONTACT_STORE,
                        "source", source,
                        NULL);
  g_ptr_array_add (contacts->stores, store);
  g_signal_emit (G_OBJECT (contacts), signals [STORE_ADDED], 0, store);

  return store;
}

/**
 * valent_contacts_get_store:
 * @contacts: a #ValentContacts
 * @uid: an address book id
 *
 * Get a #ValentContactStore for @uid.
 *
 * Returns: (transfer none) (nullable): a #ValentContactStore
 */
ValentContactStore *
valent_contacts_get_store (ValentContacts *contacts,
                           const char     *uid)
{
  g_return_val_if_fail (VALENT_IS_CONTACTS (contacts), NULL);
  g_return_val_if_fail (uid != NULL, NULL);

  for (unsigned int i = 0; i < contacts->stores->len; i++)
    {
      ValentContactStore *store = g_ptr_array_index (contacts->stores, i);

      if (g_strcmp0 (valent_contact_store_get_uid (store), uid) == 0)
        return store;
    }

  return NULL;
}

/**
 * valent_contacts_get_stores:
 * @contacts: a #ValentContacts
 *
 * Get a list of the contact stores known to @contacts.
 *
 * Returns: (transfer container) (element-type Valent.ContactStore):
 *   a #GPtrArray of #ValentContactStore
 */
GPtrArray *
valent_contacts_get_stores (ValentContacts *contacts)
{
  g_autoptr (GPtrArray) stores = NULL;

  g_return_val_if_fail (VALENT_IS_CONTACTS (contacts), NULL);

  stores = g_ptr_array_new_with_free_func (g_object_unref);

  for (unsigned int i = 0; i < contacts->stores->len; i++)
    g_ptr_array_add (stores, g_object_ref (g_ptr_array_index (contacts->stores, i)));

  return g_steal_pointer (&stores);
}

