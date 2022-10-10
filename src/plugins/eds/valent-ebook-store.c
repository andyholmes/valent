// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-ebook-store"

#include "config.h"

#include <libvalent-core.h>
#include <libvalent-contacts.h>

#include "valent-ebook-store.h"
#include "valent-eds-utils.h"

#define WAIT_FOR_CONNECTED_TIMEOUT 30


struct _ValentEBookStore
{
  ValentContactStore  parent_instance;

  EBookClient        *client;
  EBookClientView    *view;
};

G_DEFINE_TYPE (ValentEBookStore, valent_ebook_store, VALENT_TYPE_CONTACT_STORE)

enum {
  PROP_0,
  PROP_CLIENT,
  N_PROPERTIES
};

static GParamSpec *properties [N_PROPERTIES];


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
  GError *error = NULL;

  if (!e_book_client_add_contacts_finish (client, result, NULL, &error))
    return g_task_return_error (task, error);

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

  g_assert (VALENT_IS_CONTACT_STORE (store));
  g_assert (contacts != NULL);
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (store, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_ebook_store_add_contacts);

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
  GError *error = NULL;

  if (!e_book_client_remove_contacts_finish (client, result, &error))
    return g_task_return_error (task, error);

  g_task_return_boolean (task, TRUE);
}

static void
valent_ebook_store_remove_contacts (ValentContactStore  *store,
                                    GSList              *uids,
                                    GCancellable        *cancellable,
                                    GAsyncReadyCallback  callback,
                                    gpointer             user_data)
{
  ValentEBookStore *self = VALENT_EBOOK_STORE (store);
  g_autoptr (GTask) task = NULL;

  g_assert (VALENT_IS_CONTACT_STORE (store));
  g_assert (uids != NULL);
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (store, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_ebook_store_remove_contacts);

  e_book_client_remove_contacts (self->client,
                                 uids,
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

static void
on_objects_added (EBookClientView    *view,
                  GSList             *contacts,
                  ValentContactStore *store)
{
  g_assert (E_IS_BOOK_CLIENT_VIEW (view));
  g_assert (VALENT_IS_CONTACT_STORE (store));

  for (const GSList *iter = contacts; iter; iter = iter->next)
    valent_contact_store_contact_added (store, E_CONTACT (iter->data));
}

static void
on_objects_removed (EBookClientView    *view,
                    GSList             *uids,
                    ValentContactStore *store)
{
  g_assert (E_IS_BOOK_CLIENT_VIEW (view));
  g_assert (VALENT_IS_CONTACT_STORE (store));

  for (const GSList *iter = uids; iter; iter = iter->next)
    valent_contact_store_contact_removed (store, (const char *)iter->data);
}

static void
e_client_wait_for_connected_cb (EClient            *client,
                                GAsyncResult       *result,
                                ValentContactStore *store)
{
  g_autoptr (GError) error = NULL;

  if (!e_client_wait_for_connected_finish (client, result, &error) &&
      !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
      g_debug ("%s (%s): failed to connect backend: %s",
               G_OBJECT_TYPE_NAME (store),
               valent_contact_store_get_name (store),
               error->message);
    }
}

static void
e_book_client_get_view_cb (EBookClient      *client,
                           GAsyncResult     *result,
                           ValentEBookStore *self)
{
  g_autoptr (EBookClientView) view = NULL;
  g_autoptr (GCancellable) cancellable = NULL;
  g_autoptr (GError) error = NULL;

  /* If the operation succeeds, we can connect our signal handlers */
  if (e_book_client_get_view_finish (client, result, &view, &error))
    {
      self->view = g_steal_pointer (&view);
      g_signal_connect (self->view,
                        "objects-added",
                        G_CALLBACK (on_objects_added),
                        self);
      g_signal_connect (self->view,
                        "objects-removed",
                        G_CALLBACK (on_objects_removed),
                        self);
    }

  /* If the operation was cancelled then our store is being unloaded, but
   * otherwise we just warn that there will be no change notifications */
  else if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
      g_warning ("%s (%s): failed to subscribe to changes: %s",
                 G_OBJECT_TYPE_NAME (self),
                 valent_contact_store_get_name (VALENT_CONTACT_STORE (self)),
                 error->message);
    }
  else
    return;

  /* Now that we've done our setup, we can connect the backend */
  cancellable = valent_object_ref_cancellable (VALENT_OBJECT (self));
  e_client_wait_for_connected (E_CLIENT (self->client),
                               WAIT_FOR_CONNECTED_TIMEOUT,
                               cancellable,
                               (GAsyncReadyCallback)e_client_wait_for_connected_cb,
                               self);
}

/*
 * GObject
 */
static void
valent_ebook_store_constructed (GObject *object)
{
  ValentEBookStore *self = VALENT_EBOOK_STORE (object);
  g_autoptr (GCancellable) cancellable = NULL;

  g_assert (E_IS_BOOK_CLIENT (self->client));

  /* Get an unfiltered view so that we can forward change notifications */
  cancellable = valent_object_ref_cancellable (VALENT_OBJECT (self));
  e_book_client_get_view (self->client,
                          "",
                          cancellable,
                          (GAsyncReadyCallback)e_book_client_get_view_cb,
                          self);

  G_OBJECT_CLASS (valent_ebook_store_parent_class)->constructed (object);
}

static void
valent_ebook_store_dispose (GObject *object)
{
  ValentEBookStore *self = VALENT_EBOOK_STORE (object);

  if (self->view != NULL)
    {
      g_signal_handlers_disconnect_by_data (self->view, self);
      g_clear_object (&self->view);
    }

  G_OBJECT_CLASS (valent_ebook_store_parent_class)->dispose (object);
}

static void
valent_ebook_store_finalize (GObject *object)
{
  ValentEBookStore *self = VALENT_EBOOK_STORE (object);

  g_clear_object (&self->view);
  g_clear_object (&self->client);

  G_OBJECT_CLASS (valent_ebook_store_parent_class)->finalize (object);
}

static void
valent_ebook_store_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  ValentEBookStore *self = VALENT_EBOOK_STORE (object);

  switch (prop_id)
    {
    case PROP_CLIENT:
      g_value_set_object (value, self->client);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_ebook_store_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  ValentEBookStore *self = VALENT_EBOOK_STORE (object);

  switch (prop_id)
    {
    case PROP_CLIENT:
      self->client = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_ebook_store_class_init (ValentEBookStoreClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentContactStoreClass *store_class = VALENT_CONTACT_STORE_CLASS (klass);

  object_class->constructed = valent_ebook_store_constructed;
  object_class->dispose = valent_ebook_store_dispose;
  object_class->finalize = valent_ebook_store_finalize;
  object_class->get_property = valent_ebook_store_get_property;
  object_class->set_property = valent_ebook_store_set_property;

  store_class->add_contacts = valent_ebook_store_add_contacts;
  store_class->remove_contacts = valent_ebook_store_remove_contacts;
  store_class->get_contact = valent_ebook_store_get_contact;
  store_class->query = valent_ebook_store_query;

  /**
   * ValentEBookStore:client:
   *
   * The #EBookClient for the store.
   */
  properties [PROP_CLIENT] =
    g_param_spec_object ("client", NULL, NULL,
                         E_TYPE_BOOK_CLIENT,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
valent_ebook_store_init (ValentEBookStore *self)
{
}

