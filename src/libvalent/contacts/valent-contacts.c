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
};

G_DEFINE_FINAL_TYPE (ValentContacts, valent_contacts, VALENT_TYPE_COMPONENT)

/*
 * GObject
 */
static void
valent_contacts_class_init (ValentContactsClass *klass)
{
}

static void
valent_contacts_init (ValentContacts *self)
{
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
  static ValentContacts *default_instance = NULL;

  if (default_instance == NULL)
    {
      default_instance = g_object_new (VALENT_TYPE_CONTACTS,
                                       "plugin-domain", "contacts",
                                       "plugin-type",   VALENT_TYPE_CONTACTS_ADAPTER,
                                       NULL);
      g_object_add_weak_pointer (G_OBJECT (default_instance),
                                 (gpointer)&default_instance);
    }

  return default_instance;
}

