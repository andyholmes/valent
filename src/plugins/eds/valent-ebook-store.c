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
static void
valent_ebook_store_add_contacts_cb (GObject      *object,
                                    GAsyncResult *result,
                                    gpointer      user_data)
{
  EBookClient *client = E_BOOK_CLIENT (object);
  g_autoptr (GTask) task = G_TASK (user_data);
  ValentContactStore *store = g_task_get_source_object (task);
  GSList *additions = g_task_get_task_data (task);
  GError *error = NULL;

  if (!e_book_client_add_contact_finish (client, result, NULL, &error))
    return g_task_return_error (task, error);

  for (const GSList *iter = additions; iter; iter = iter->next)
    valent_contact_store_emit_contact_added (store, E_CONTACT (iter->data));

  g_task_return_boolean (task, TRUE);
}

static void
valent_ebook_store_add_contacts (ValentContactStore  *store,
                                 GSList              *contacts,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
  ValentEBookStore *self = VALENT_EBOOK_STORE (store);
  g_autoptr (GTask) task = NULL;
  GSList *additions = NULL;

  g_assert (VALENT_IS_CONTACT_STORE (store));
  g_assert (contacts != NULL);
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  for (const GSList *iter = contacts; iter; iter = iter->next)
    additions = g_slist_append (additions, g_object_ref (iter->data));

  task = g_task_new (store, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_ebook_store_add_contacts);
  g_task_set_task_data (task, additions, valent_object_slist_free);

  e_book_client_add_contacts (self->client,
                              contacts,
                              E_BOOK_OPERATION_FLAG_CONFLICT_USE_NEWER,
                              cancellable,
                              valent_ebook_store_add_contacts_cb,
                              g_steal_pointer (&task));
}

static void
valent_ebook_store_remove_cb (GObject      *object,
                              GAsyncResult *result,
                              gpointer      user_data)
{
  EBookClient *client = E_BOOK_CLIENT (object);
  g_autoptr (GTask) task = G_TASK (user_data);
  ValentContactStore *store = g_task_get_source_object (task);
  const char *uid = g_task_get_task_data (task);
  GError *error = NULL;

  if (!e_book_client_remove_contact_by_uid_finish (client, result, &error))
    return g_task_return_error (task, error);

  valent_contact_store_emit_contact_removed (store, uid);

  g_task_return_boolean (task, TRUE);
}

static void
valent_ebook_store_remove_contact (ValentContactStore  *store,
                                   const char          *uid,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data)
{
  ValentEBookStore *self = VALENT_EBOOK_STORE (store);
  g_autoptr (GTask) task = NULL;

  g_assert (VALENT_IS_CONTACT_STORE (store));
  g_assert (uid != NULL);
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (store, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_ebook_store_remove_contact);
  g_task_set_task_data (task, g_strdup (uid), g_free);

  e_book_client_remove_contact_by_uid (self->client,
                                       uid,
                                       E_BOOK_OPERATION_FLAG_NONE,
                                       cancellable,
                                       valent_ebook_store_remove_cb,
                                       g_steal_pointer (&task));
}

static void
valent_ebook_store_get_contact_cb (GObject      *object,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  EBookClient *client = E_BOOK_CLIENT (object);
  g_autoptr (GTask) task = G_TASK (user_data);
  g_autoptr (EContact) contact = NULL;
  GError *error = NULL;

  if (!e_book_client_get_contact_finish (client, result, &contact, &error))
    return g_task_return_error (task, error);

  g_task_return_pointer (task, g_steal_pointer (&contact), g_object_unref);
}

static void
valent_ebook_store_get_contact (ValentContactStore  *store,
                                const char          *uid,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  ValentEBookStore *self = VALENT_EBOOK_STORE (store);
  g_autoptr (GTask) task = NULL;

  g_assert (VALENT_IS_CONTACT_STORE (store));
  g_assert (uid != NULL);
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (store, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_ebook_store_get_contact);

  e_book_client_get_contact (self->client,
                             uid,
                             cancellable,
                             valent_ebook_store_get_contact_cb,
                             g_steal_pointer (&task));
}

static void
valent_ebook_store_query_cb (GObject      *object,
                             GAsyncResult *result,
                             gpointer      user_data)
{
  EBookClient *client = E_BOOK_CLIENT (object);
  g_autoptr (GTask) task = G_TASK (user_data);
  GSList *results = NULL;
  GError *error = NULL;

  if (!e_book_client_get_contacts_finish (client, result, &results, &error))
    return g_task_return_error (task, error);

  g_task_return_pointer (task, g_steal_pointer (&results), g_object_unref);
}

static void
valent_ebook_store_query (ValentContactStore  *store,
                          const char          *query,
                          GCancellable        *cancellable,
                          GAsyncReadyCallback  callback,
                          gpointer             user_data)
{
  ValentEBookStore *self = VALENT_EBOOK_STORE (store);
  g_autoptr (GTask) task = NULL;

  g_assert (VALENT_IS_CONTACT_STORE (store));
  g_assert (query != NULL);
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (store, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_ebook_store_query);
  g_task_set_task_data (task, g_strdup (query), g_free);

  e_book_client_get_contacts (self->client,
                              query,
                              cancellable,
                              valent_ebook_store_query_cb,
                              g_steal_pointer (&task));
}

/*
 * GObject
 */
static void
valent_ebook_store_constructed (GObject *object)
{
  ValentEBookStore *self = VALENT_EBOOK_STORE (object);
  ValentContactStore *store = VALENT_CONTACT_STORE (object);
  ESource *source;
  g_autoptr (GError) error = NULL;
  g_autoptr (EClient) client = NULL;

  /* Connect the client */
  source = valent_contact_store_get_source (store);
  client = e_book_client_connect_sync (source, 30, NULL, &error);

  if (client == NULL)
    g_warning ("Failed to connect EClient: %s", error->message);
  else
    g_set_object (&self->client, E_BOOK_CLIENT (client));

  G_OBJECT_CLASS (valent_ebook_store_parent_class)->constructed (object);
}

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

  object_class->constructed = valent_ebook_store_constructed;
  object_class->finalize = valent_ebook_store_finalize;

  store_class->add_contacts = valent_ebook_store_add_contacts;
  store_class->remove_contact = valent_ebook_store_remove_contact;
  store_class->get_contact = valent_ebook_store_get_contact;
  store_class->query = valent_ebook_store_query;
}

