// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-contacts"

#include "config.h"

#include <gio/gio.h>
#include <libpeas/peas.h>
#include <libvalent-core.h>

#include "valent-contacts.h"
#include "valent-contacts-adapter.h"
#include "valent-contact-store.h"
#include "valent-contact-cache.h"


/**
 * ValentContacts:
 *
 * A class for managing address books.
 *
 * #ValentContacts is an address book manager, intended for use by
 * [class@Valent.DevicePlugin] implementations.
 *
 * Plugins can implement [class@Valent.ContactsAdapter] to provide an interface
 * to manage instances of [class@Valent.ContactStore]. If no adapter is
 * available, #ValentContacts will create [class@Valent.ContactCache] as
 * necessary.
 *
 * Since: 1.0
 */

struct _ValentContacts
{
  ValentComponent  parent_instance;

  GCancellable    *cancellable;
  GPtrArray       *stores;
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
 * ValentContactsAdapter Callbacks
 */
static void
on_store_added (ValentContactsAdapter *adapter,
                ValentContactStore    *store,
                ValentContacts        *self)
{
  VALENT_ENTRY;

  g_assert (VALENT_IS_CONTACTS (self));

  VALENT_NOTE ("%s: %s",
               G_OBJECT_TYPE_NAME (store),
               valent_contact_store_get_name (store));

  g_ptr_array_add (self->stores, g_object_ref (store));
  g_signal_emit (G_OBJECT (self), signals [STORE_ADDED], 0, store);

  VALENT_EXIT;
}

static void
on_store_removed (ValentContactsAdapter *adapter,
                  ValentContactStore    *store,
                  ValentContacts        *self)
{
  VALENT_ENTRY;

  g_assert (VALENT_IS_CONTACTS (self));

  VALENT_NOTE ("%s: %s",
               G_OBJECT_TYPE_NAME (store),
               valent_contact_store_get_name (store));

  g_ptr_array_remove (self->stores, store);
  g_signal_emit (G_OBJECT (self), signals [STORE_REMOVED], 0, store);

  VALENT_EXIT;
}

static void
valent_contacts_adapter_load_cb (ValentContactsAdapter *adapter,
                                 GAsyncResult          *result,
                                 ValentContacts        *contacts)
{
  g_autoptr (GError) error = NULL;

  VALENT_ENTRY;

  if (!valent_contacts_adapter_load_finish (adapter, result, &error) &&
      !valent_error_ignore (error))
    g_warning ("%s: %s", G_OBJECT_TYPE_NAME (adapter), error->message);

  VALENT_EXIT;
}


/*
 * ValentComponent
 */
static void
valent_contacts_enable_extension (ValentComponent *component,
                                  PeasExtension   *extension)
{
  ValentContacts *self = VALENT_CONTACTS (component);
  ValentContactsAdapter *adapter = VALENT_CONTACTS_ADAPTER (extension);

  VALENT_ENTRY;

  g_assert (VALENT_IS_CONTACTS (self));
  g_assert (VALENT_IS_CONTACTS_ADAPTER (adapter));


  g_signal_connect_object (adapter,
                           "store-added",
                           G_CALLBACK (on_store_added),
                           self,
                           0);

  g_signal_connect_object (adapter,
                           "store-removed",
                           G_CALLBACK (on_store_removed),
                           self,
                           0);

  valent_contacts_adapter_load_async (adapter,
                                      self->cancellable,
                                      (GAsyncReadyCallback)valent_contacts_adapter_load_cb,
                                      self);

  VALENT_EXIT;
}

static void
valent_contacts_disable_extension (ValentComponent *component,
                                   PeasExtension   *extension)
{
  ValentContacts *self = VALENT_CONTACTS (component);
  ValentContactsAdapter *adapter = VALENT_CONTACTS_ADAPTER (extension);
  g_autoptr (GPtrArray) stores = NULL;

  VALENT_ENTRY;

  g_assert (VALENT_IS_CONTACTS (self));
  g_assert (VALENT_IS_CONTACTS_ADAPTER (adapter));

  /* Simulate removal */
  stores = valent_contacts_adapter_get_stores (adapter);

  for (unsigned int i = 0; i < stores->len; i++)
    valent_contacts_adapter_emit_store_removed (adapter, g_ptr_array_index (stores, i));

  g_signal_handlers_disconnect_by_func (adapter, on_store_added, self);
  g_signal_handlers_disconnect_by_func (adapter, on_store_removed, self);

  VALENT_EXIT;
}

/*
 * ValentContacts
 */
static ValentContactStore *
valent_contacts_create_store (const char *uid,
                              const char *name,
                              const char *icon_name)
{
  g_autoptr (ESource) source = NULL;
  ESourceBackend *backend;

  g_assert (uid != NULL && *uid != '\0');
  g_assert (name != NULL && *name != '\0');

  /* Create a scratch source for a local addressbook source */
  source = e_source_new_with_uid (uid, NULL, NULL);
  backend = e_source_get_extension (source, E_SOURCE_EXTENSION_ADDRESS_BOOK);
  e_source_backend_set_backend_name (backend, "local");
  e_source_set_display_name (source, name);

  return g_object_new (VALENT_TYPE_CONTACT_CACHE,
                       "source", source,
                       NULL);
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

  component_class->enable_extension = valent_contacts_enable_extension;
  component_class->disable_extension = valent_contacts_disable_extension;

  /**
   * ValentContacts::store-added:
   * @contacts: a #ValentContacts
   * @store: a #ValentContactStore
   *
   * Emitted when a [class@Valent.ContactStore] is added to a
   * [class@Valent.ContactsAdapter].
   *
   * Since: 1.0
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
   * Emitted when a [class@Valent.ContactStore] is removed from a
   * [class@Valent.ContactsAdapter].
   *
   * Since: 1.0
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
 * Get the default [class@Valent.Contacts].
 *
 * Returns: (transfer none) (not nullable): a #ValentContacts
 *
 * Since: 1.0
 */
ValentContacts *
valent_contacts_get_default (void)
{
  if (default_contacts == NULL)
    {
      default_contacts = g_object_new (VALENT_TYPE_CONTACTS,
                                       "plugin-context",  "contacts",
                                       "plugin-priority", "ContactsAdapterPriority",
                                       "plugin-type",     VALENT_TYPE_CONTACTS_ADAPTER,
                                       NULL);

      g_object_add_weak_pointer (G_OBJECT (default_contacts),
                                 (gpointer)&default_contacts);
    }

  return default_contacts;
}

/**
 * valent_contacts_ensure_store:
 * @contacts: (nullable): a #ValentContacts
 * @uid: a unique id
 * @name: a display name
 *
 * Get a #ValentContactStore for @uid.
 *
 * If the contact store does not exist, one will be created using the default
 * adapter and passed @name and @description.
 *
 * Returns: (transfer none) (not nullable): an address book
 *
 * Since: 1.0
 */
ValentContactStore *
valent_contacts_ensure_store (ValentContacts *contacts,
                              const char     *uid,
                              const char     *name)
{
  ValentContactStore *ret;

  VALENT_ENTRY;

  g_return_val_if_fail (uid != NULL && *uid != '\0', NULL);
  g_return_val_if_fail (name != NULL && *name != '\0', NULL);

  /* Use the default ValentContacts */
  if (contacts == NULL)
    contacts = valent_contacts_get_default ();

  /* Try to find an existing store */
  if ((ret = valent_contacts_get_store (contacts, uid)) != NULL)
    VALENT_RETURN (ret);

  /* Create a new store */
  ret = valent_contacts_create_store (uid, name, NULL);
  g_ptr_array_add (contacts->stores, ret);
  g_signal_emit (G_OBJECT (contacts), signals [STORE_ADDED], 0, ret);

  VALENT_RETURN (ret);
}

/**
 * valent_contacts_get_store:
 * @contacts: a #ValentContacts
 * @uid: an address book id
 *
 * Get a #ValentContactStore for @uid.
 *
 * Returns: (transfer none) (nullable): an address book, or %NULL if not found
 *
 * Since: 1.0
 */
ValentContactStore *
valent_contacts_get_store (ValentContacts *contacts,
                           const char     *uid)
{
  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_CONTACTS (contacts), NULL);
  g_return_val_if_fail (uid != NULL, NULL);

  for (unsigned int i = 0; i < contacts->stores->len; i++)
    {
      ValentContactStore *store = g_ptr_array_index (contacts->stores, i);

      if (g_strcmp0 (valent_contact_store_get_uid (store), uid) == 0)
        VALENT_RETURN (store);
    }

  VALENT_RETURN (NULL);
}

/**
 * valent_contacts_get_stores:
 * @contacts: a #ValentContacts
 *
 * Get a list of the contact stores known to @contacts.
 *
 * Returns: (transfer container) (element-type Valent.ContactStore): a list of
 *     address books
 *
 * Since: 1.0
 */
GPtrArray *
valent_contacts_get_stores (ValentContacts *contacts)
{
  GPtrArray *ret;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_CONTACTS (contacts), NULL);

  ret = g_ptr_array_new_with_free_func (g_object_unref);

  for (unsigned int i = 0; i < contacts->stores->len; i++)
    g_ptr_array_add (ret, g_object_ref (g_ptr_array_index (contacts->stores, i)));

  VALENT_RETURN (ret);
}

