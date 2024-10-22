// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-contacts"

#include "config.h"

#include <gio/gio.h>
#include <libpeas.h>
#include <libvalent-core.h>

#include "valent-contacts.h"
#include "valent-contacts-adapter.h"


/**
 * ValentContacts:
 *
 * A class for managing address books.
 *
 * `ValentContacts` is an address book manager, intended for use by
 * [class@Valent.DevicePlugin] implementations.
 *
 * Plugins can implement [class@Valent.ContactsAdapter] to provide an interface
 * to manage address books, or lists of contacts.
 *
 * Since: 1.0
 */

struct _ValentContacts
{
  ValentComponent  parent_instance;

  /* list model */
  GPtrArray       *items;
};

static void   valent_contacts_unbind_extension (ValentComponent *component,
                                                GObject         *extension);
static void   g_list_model_iface_init          (GListModelInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (ValentContacts, valent_contacts, VALENT_TYPE_COMPONENT,
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

  if G_UNLIKELY (position >= self->items->len)
    return NULL;

  return g_object_ref (g_ptr_array_index (self->items, position));
}

static GType
valent_contacts_get_item_type (GListModel *list)
{
  return VALENT_TYPE_CONTACTS_ADAPTER;
}

static unsigned int
valent_contacts_get_n_items (GListModel *list)
{
  ValentContacts *self = VALENT_CONTACTS (list);

  g_assert (VALENT_IS_CONTACTS (self));

  return self->items->len;
}

static void
g_list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item = valent_contacts_get_item;
  iface->get_item_type = valent_contacts_get_item_type;
  iface->get_n_items = valent_contacts_get_n_items;
}

/*
 * ValentComponent
 */
static void
valent_contacts_bind_extension (ValentComponent *component,
                                GObject         *extension)
{
  ValentContacts *self = VALENT_CONTACTS (component);
  unsigned int position = 0;

  VALENT_ENTRY;

  g_assert (VALENT_IS_CONTACTS (self));
  g_assert (VALENT_IS_CONTACTS_ADAPTER (extension));

  if (g_ptr_array_find (self->items, extension, &position))
    {
      g_warning ("Adapter \"%s\" already exported in \"%s\"",
                 G_OBJECT_TYPE_NAME (extension),
                 G_OBJECT_TYPE_NAME (component));
      return;
    }

  g_signal_connect_object (extension,
                           "destroy",
                           G_CALLBACK (valent_contacts_unbind_extension),
                           self,
                           G_CONNECT_SWAPPED);

  position = self->items->len;
  g_ptr_array_add (self->items, g_object_ref (extension));
  g_list_model_items_changed (G_LIST_MODEL (self), position, 0, 1);

  VALENT_EXIT;
}

static void
valent_contacts_unbind_extension (ValentComponent *component,
                                  GObject         *extension)
{
  ValentContacts *self = VALENT_CONTACTS (component);
  g_autoptr (ValentExtension) item = NULL;
  unsigned int position = 0;

  VALENT_ENTRY;

  g_assert (VALENT_IS_CONTACTS (self));
  g_assert (VALENT_IS_CONTACTS_ADAPTER (extension));

  if (!g_ptr_array_find (self->items, extension, &position))
    {
      g_warning ("Adapter \"%s\" not found in \"%s\"",
                 G_OBJECT_TYPE_NAME (extension),
                 G_OBJECT_TYPE_NAME (component));
      return;
    }

  g_signal_handlers_disconnect_by_func (extension, valent_contacts_unbind_extension, self);
  item = g_ptr_array_steal_index (self->items, position);
  g_list_model_items_changed (G_LIST_MODEL (self), position, 1, 0);

  VALENT_EXIT;
}

/*
 * GObject
 */
static void
valent_contacts_finalize (GObject *object)
{
  ValentContacts *self = VALENT_CONTACTS (object);

  g_clear_pointer (&self->items, g_ptr_array_unref);

  G_OBJECT_CLASS (valent_contacts_parent_class)->finalize (object);
}

static void
valent_contacts_class_init (ValentContactsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentComponentClass *component_class = VALENT_COMPONENT_CLASS (klass);

  object_class->finalize = valent_contacts_finalize;

  component_class->bind_extension = valent_contacts_bind_extension;
  component_class->unbind_extension = valent_contacts_unbind_extension;
}

static void
valent_contacts_init (ValentContacts *self)
{
  self->items = g_ptr_array_new_with_free_func (g_object_unref);
}

/**
 * valent_contacts_get_default:
 *
 * Get the default [class@Valent.Contacts].
 *
 * Returns: (transfer none) (not nullable): a `ValentContacts`
 *
 * Since: 1.0
 */
ValentContacts *
valent_contacts_get_default (void)
{
  if (default_contacts == NULL)
    {
      default_contacts = g_object_new (VALENT_TYPE_CONTACTS,
                                       "plugin-domain", "contacts",
                                       "plugin-type",   VALENT_TYPE_CONTACTS_ADAPTER,
                                       NULL);

      g_object_add_weak_pointer (G_OBJECT (default_contacts),
                                 (gpointer)&default_contacts);
    }

  return default_contacts;
}

/**
 * valent_contacts_export_adapter:
 * @contacts: a `ValentContacts`
 * @object: a `ValentContactsAdapter`
 *
 * Export @object on all adapters that support it.
 *
 * Since: 1.0
 */
void
valent_contacts_export_adapter (ValentContacts        *contacts,
                                ValentContactsAdapter *object)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_CONTACTS (contacts));
  g_return_if_fail (VALENT_IS_CONTACTS_ADAPTER (object));

  valent_contacts_bind_extension (VALENT_COMPONENT (contacts),
                                  G_OBJECT (object));

  VALENT_EXIT;
}

/**
 * valent_contacts_unexport_adapter:
 * @contacts: a `ValentContacts`
 * @object: a `ValentContactsAdapter`
 *
 * Unexport @object from all adapters that support it.
 *
 * Since: 1.0
 */
void
valent_contacts_unexport_adapter (ValentContacts        *contacts,
                                  ValentContactsAdapter *object)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_CONTACTS (contacts));
  g_return_if_fail (VALENT_IS_CONTACTS_ADAPTER (object));

  valent_contacts_unbind_extension (VALENT_COMPONENT (contacts),
                                    G_OBJECT (object));

  VALENT_EXIT;
}

