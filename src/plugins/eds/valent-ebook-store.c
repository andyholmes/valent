// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-ebook-store"

#include "config.h"

#include <libvalent-core.h>
#include <libvalent-contacts.h>

#include "valent-ebook-store.h"
#include "valent-eds-utils.h"


struct _ValentEBookStore
{
  ValentContactStore  parent_instance;

  EBookClient        *client;
};

G_DEFINE_TYPE (ValentEBookStore, valent_ebook_store, VALENT_TYPE_CONTACT_STORE)


/*
 * ValentContactStore
 */
static gboolean
valent_ebook_store_add (ValentContactStore  *store,
                        EContact            *contact,
                        GCancellable        *cancellable,
                        GError             **error)
{
  ValentEBookStore *self  = VALENT_EBOOK_STORE (store);

  g_assert (VALENT_IS_EBOOK_STORE (store));
  g_assert (E_IS_CONTACT (contact));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_assert (error == NULL || *error == NULL);

  return e_book_client_add_contact_sync (self->client,
                                         contact,
                                         E_BOOK_OPERATION_FLAG_CONFLICT_USE_NEWER,
                                         NULL,
                                         cancellable,
                                         error);
}

static EContact *
valent_ebook_store_get_contact (ValentContactStore  *store,
                                const char          *uid,
                                GCancellable        *cancellable,
                                GError             **error)
{
  ValentEBookStore *self = VALENT_EBOOK_STORE (store);
  EContact *contact = NULL;

  g_assert (VALENT_IS_EBOOK_STORE (store));
  g_assert (uid != NULL);
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_assert (error == NULL || *error == NULL);

  if (!e_book_client_get_contact_sync (self->client, uid, &contact, cancellable, error))
    return NULL;

  return contact;
}

static gboolean
valent_ebook_store_remove (ValentContactStore  *store,
                           const char          *uid,
                           GCancellable        *cancellable,
                           GError             **error)
{
  ValentEBookStore *self  = VALENT_EBOOK_STORE (store);

  g_assert (VALENT_IS_EBOOK_STORE (store));
  g_assert (uid != NULL);
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_assert (error == NULL || *error == NULL);

  return e_book_client_remove_contact_by_uid_sync (self->client,
                                                   uid,
                                                   E_BOOK_OPERATION_FLAG_NONE,
                                                   cancellable,
                                                   error);
}

static GSList *
valent_ebook_store_query (ValentContactStore  *store,
                          const char          *query,
                          GCancellable        *cancellable,
                          GError             **error)
{
  ValentEBookStore *self = VALENT_EBOOK_STORE (store);
  GSList *results = NULL;

  g_assert (VALENT_IS_EBOOK_STORE (store));
  g_assert (query != NULL);
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_assert (error == NULL || *error == NULL);

  e_book_client_get_contacts_sync (self->client,
                                   query,
                                   &results,
                                   cancellable,
                                   error);

  return results;
}

static void
valent_ebook_store_prepare_backend (ValentContactStore *store)
{
  ValentEBookStore *self = VALENT_EBOOK_STORE (store);
  ESource *source;
  g_autoptr (GError) error = NULL;
  g_autoptr (EClient) client = NULL;

  g_assert (VALENT_IS_EBOOK_STORE (self));

  /* Connect the client */
  source = valent_contact_store_get_source (store);
  client = e_book_client_connect_sync (source, 30, NULL, &error);

  if (client == NULL)
    g_warning ("Failed to connect EClient: %s", error->message);
  else
    g_set_object (&self->client, E_BOOK_CLIENT (client));
}

/*
 * GObject
 */
static void
valent_ebook_store_finalize (GObject *object)
{
  ValentEBookStore *self = VALENT_EBOOK_STORE (object);

  g_clear_object (&self->client);

  G_OBJECT_CLASS (valent_ebook_store_parent_class)->finalize (object);
}

static void
valent_ebook_store_init (ValentEBookStore *store)
{
}

static void
valent_ebook_store_class_init (ValentEBookStoreClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentContactStoreClass *store_class = VALENT_CONTACT_STORE_CLASS (klass);

  object_class->finalize = valent_ebook_store_finalize;

  store_class->add = valent_ebook_store_add;
  store_class->remove = valent_ebook_store_remove;
  store_class->get_contact = valent_ebook_store_get_contact;
  store_class->query = valent_ebook_store_query;
  store_class->prepare_backend = valent_ebook_store_prepare_backend;
}

