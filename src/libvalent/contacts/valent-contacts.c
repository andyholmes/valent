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
#include "valent-contact-cache-private.h"


/**
 * ValentContacts:
 *
 * A class for managing address books.
 *
 * #ValentContacts is an address book manager, intended for use by
 * [class@Valent.DevicePlugin] implementations.
 *
 * Plugins can implement [class@Valent.ContactsAdapter] to provide an interface
 * to manage instances of [class@Valent.ContactStore].
 *
 * Since: 1.0
 */

struct _ValentContacts
{
  ValentComponent  parent_instance;

  GPtrArray       *stores;
};

static void   g_list_model_iface_init (GListModelInterface *iface);

G_DEFINE_TYPE_WITH_CODE (ValentContacts, valent_contacts, VALENT_TYPE_COMPONENT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, g_list_model_iface_init))

static ValentContacts *default_contacts = NULL;


/*
 * GListModel
 */
static gpointer
valent_contacts_get_item (GListModel   *list,
                          unsigned int  position)
{
  ValentContacts *self = VALENT_CONTACTS (list);

  g_assert (VALENT_IS_CONTACTS (self));
  g_assert (position < self->stores->len);

  return g_object_ref (g_ptr_array_index (self->stores, position));
}

static GType
valent_contacts_get_item_type (GListModel *list)
{
  return VALENT_TYPE_CONTACT_STORE;
}

static unsigned int
valent_contacts_get_n_items (GListModel *list)
{
  ValentContacts *self = VALENT_CONTACTS (list);

  g_assert (VALENT_IS_CONTACTS (self));

  return self->stores->len;
}

static void
g_list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item = valent_contacts_get_item;
  iface->get_item_type = valent_contacts_get_item_type;
  iface->get_n_items = valent_contacts_get_n_items;
}


/*
 * ValentContactsAdapter Callbacks
 */
static void
on_store_added (ValentContactsAdapter *adapter,
                ValentContactStore    *store,
                ValentContacts        *self)
{
  unsigned int position = 0;

  VALENT_ENTRY;

  g_assert (VALENT_IS_CONTACTS (self));

  VALENT_NOTE ("%s: %s",
               G_OBJECT_TYPE_NAME (store),
               valent_contact_store_get_name (store));

  position = self->stores->len;
  g_ptr_array_add (self->stores, g_object_ref (store));
  g_list_model_items_changed (G_LIST_MODEL (self), position, 0, 1);

  VALENT_EXIT;
}

static void
on_store_removed (ValentContactsAdapter *adapter,
                  ValentContactStore    *store,
                  ValentContacts        *self)
{
  unsigned int position = 0;

  VALENT_ENTRY;

  g_assert (VALENT_IS_CONTACTS (self));

  VALENT_NOTE ("%s: %s",
               G_OBJECT_TYPE_NAME (store),
               valent_contact_store_get_name (store));

  if (g_ptr_array_find (self->stores, store, &position))
    {
      g_ptr_array_remove_index (self->stores, position);
      g_list_model_items_changed (G_LIST_MODEL (self), position, 1, 0);
    }

  VALENT_EXIT;
}


/*
 * ValentComponent
 */
static void
valent_contacts_bind_extension (ValentComponent *component,
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

  VALENT_EXIT;
}

static void
valent_contacts_unbind_extension (ValentComponent *component,
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

  for (unsigned int i = 0, len = stores->len; i < len; i++)
    valent_contacts_adapter_store_removed (adapter, g_ptr_array_index (stores, i));

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
valent_contacts_finalize (GObject *object)
{
  ValentContacts *self = VALENT_CONTACTS (object);

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

  object_class->finalize = valent_contacts_finalize;

  component_class->bind_extension = valent_contacts_bind_extension;
  component_class->unbind_extension = valent_contacts_unbind_extension;
}

static void
valent_contacts_init (ValentContacts *self)
{
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
 * @contacts: a #ValentContacts
 * @uid: a unique id
 * @name: a display name
 *
 * Get a #ValentContactStore for @uid.
 *
 * If the contact store does not exist, one will be created using the default
 * adapter and passed @name and @description. If no adapter is available, a new
 * file-based store will be created.
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
  unsigned int position = 0;

  VALENT_ENTRY;

  g_return_val_if_fail (uid != NULL && *uid != '\0', NULL);
  g_return_val_if_fail (name != NULL && *name != '\0', NULL);

  /* Try to find an existing store */
  for (unsigned int i = 0, len = contacts->stores->len; i < len; i++)
    {
      ret = g_ptr_array_index (contacts->stores, i);

      if (g_strcmp0 (valent_contact_store_get_uid (ret), uid) == 0)
        VALENT_RETURN (ret);
    }

  /* Create a new store */
  ret = valent_contacts_create_store (uid, name, NULL);
  position = contacts->stores->len;
  g_ptr_array_add (contacts->stores, ret);
  g_list_model_items_changed (G_LIST_MODEL (contacts), position, 0, 1);

  VALENT_RETURN (ret);
}

/**
 * valent_contacts_lookup_store:
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
valent_contacts_lookup_store (ValentContacts *contacts,
                              const char     *uid)
{
  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_CONTACTS (contacts), NULL);
  g_return_val_if_fail (uid != NULL, NULL);

  for (unsigned int i = 0, len = contacts->stores->len; i < len; i++)
    {
      ValentContactStore *store = g_ptr_array_index (contacts->stores, i);

      if (g_strcmp0 (valent_contact_store_get_uid (store), uid) == 0)
        VALENT_RETURN (store);
    }

  VALENT_RETURN (NULL);
}

