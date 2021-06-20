// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-contact-store"

#include "config.h"

#include <libvalent-core.h>

#include "valent-contact-store.h"
#include "valent-contact-utils.h"
#include "valent-eds.h"


/**
 * SECTION:valent-contact-store
 * @short_description: A helper for ESource address books
 * @title: ValentContactStore
 * @stability: Unstable
 * @include: libvalent-contacts.h
 *
 * The #ValentContactStore class is a simple implementation of #ValentContactStore
 * that is effectively a wrapper around #ESource and #EBookCache.
 */

typedef struct
{
  EBookCache *cache;
  ESource    *source;
} ValentContactStorePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (ValentContactStore, valent_contact_store, G_TYPE_OBJECT)

/**
 * ValentContactStoreClass:
 * @add: the virtual function pointer for valent_contact_store_add()
 * @add_async: the virtual function pointer for valent_contact_store_add_async()
 * @get_contact: the virtual function pointer for valent_contact_store_get_contact()
 * @get_contact_async: the virtual function pointer for valent_contact_store_get_contact_async()
 * @remove: the virtual function pointer for valent_contact_store_remove()
 * @remove_async: the virtual function pointer for valent_contact_store_remove_async()
 * @query: the virtual function pointer for valent_contact_store_query()
 * @query_async: the virtual function pointer for valent_contact_store_query_async()
 * @prepare_backend: the virtual function pointer for valent_contact_store_prepare_backend()
 * @contact_added: the class closure for #ValentContactStore::contact-added
 * @contact_removed: the class closure for #ValentContactStore::contact-removed
 *
 * The virtual function table for #ValentContactStore.
 */


enum {
  PROP_0,
  PROP_NAME,
  PROP_SOURCE,
  PROP_UID,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

enum {
  CONTACT_ADDED,
  CONTACT_REMOVED,
  N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0, };


/*
 * ECache Helpers
 */
typedef struct
{
  ValentContactStore *store;
  char               *uid;
  EContact           *contact;
  guint               signal_id;
} ChangeEmission;


static gboolean
emit_change_main (gpointer data)
{
  ChangeEmission *emission = data;

  g_assert (emission != NULL);
  g_assert (VALENT_IS_CONTACT_STORE (emission->store));
  g_assert (emission->uid != NULL);
  g_assert (emission->contact == NULL || E_IS_CONTACT (emission->contact));

  g_signal_emit (G_OBJECT (emission->store),
                 emission->signal_id, 0,
                 emission->uid,
                 emission->contact);

  g_clear_object (&emission->store);
  g_clear_pointer (&emission->uid, g_free);
  g_clear_object (&emission->contact);
  g_free (emission);

  return G_SOURCE_REMOVE;
}

/* LCOV_EXCL_START */
static gboolean
valent_contact_store_real_add (ValentContactStore  *store,
                               EContact            *contact,
                               GCancellable        *cancellable,
                               GError             **error)
{
  ValentContactStorePrivate *priv = valent_contact_store_get_instance_private (store);
  gboolean ret;

  g_assert (VALENT_IS_CONTACT_STORE (store));
  g_assert (E_IS_CONTACT (contact));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_assert (error == NULL || *error == NULL);

  ret = e_book_cache_put_contact (priv->cache,
                                  contact,
                                  NULL,
                                  0,
                                  E_CACHE_IS_OFFLINE,
                                  cancellable,
                                  error);
  if (ret)
    {
      const char *uid = e_contact_get_const (contact, E_CONTACT_UID);
      valent_contact_store_emit_contact_added (store, uid, contact);
    }

  return ret;
}

static void
add_task (GTask        *task,
          gpointer      source_object,
          gpointer      task_data,
          GCancellable *cancellable)
{
  ValentContactStore *store = source_object;
  EContact *contact = task_data;
  GError *error = NULL;

  if (g_task_return_error_if_cancelled (task))
    return;

  if (!valent_contact_store_add (store, contact, cancellable, &error))
    return g_task_return_error (task, error);

  g_task_return_boolean (task, TRUE);
}

static void
valent_contact_store_real_add_async (ValentContactStore  *store,
                                     EContact            *contact,
                                     GCancellable        *cancellable,
                                     GAsyncReadyCallback  callback,
                                     gpointer             user_data)
{
  g_autoptr (GTask) task = NULL;

  g_assert (VALENT_IS_CONTACT_STORE (store));
  g_assert (E_IS_CONTACT (contact));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (store, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_contact_store_real_add_async);
  g_task_set_task_data (task, g_object_ref (contact), g_object_unref);
  g_task_run_in_thread (task, add_task);
}

static gboolean
valent_contact_store_real_remove (ValentContactStore  *store,
                                  const char          *uid,
                                  GCancellable        *cancellable,
                                  GError             **error)
{
  ValentContactStorePrivate *priv = valent_contact_store_get_instance_private (store);
  gboolean ret;

  g_assert (VALENT_IS_CONTACT_STORE (store));
  g_assert (uid != NULL);
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_assert (error == NULL || *error == NULL);

  ret = e_book_cache_remove_contact (priv->cache,
                                     uid,
                                     0,
                                     E_CACHE_IS_OFFLINE,
                                     cancellable,
                                     error);

  if (ret)
    valent_contact_store_emit_contact_removed (store, uid, NULL);

  return ret;
}

static void
remove_task (GTask        *task,
             gpointer      source_object,
             gpointer      task_data,
             GCancellable *cancellable)
{
  ValentContactStore *store = source_object;
  const char *uid = task_data;
  GError *error = NULL;

  if (g_task_return_error_if_cancelled (task))
    return;

  if (!valent_contact_store_remove (store, uid, cancellable, &error))
    return g_task_return_error (task, error);

  g_task_return_boolean (task, TRUE);
}

static void
valent_contact_store_real_remove_async (ValentContactStore  *store,
                                        const char          *uid,
                                        GCancellable        *cancellable,
                                        GAsyncReadyCallback  callback,
                                        gpointer             user_data)
{
  g_autoptr (GTask) task = NULL;

  g_assert (VALENT_IS_CONTACT_STORE (store));
  g_assert (uid != NULL);
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (store, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_contact_store_real_remove_async);
  g_task_set_task_data (task, g_strdup (uid), g_free);
  g_task_run_in_thread (task, remove_task);
}

static EContact *
valent_contact_store_real_get_contact (ValentContactStore  *store,
                                       const char          *uid,
                                       GCancellable        *cancellable,
                                       GError             **error)
{
  ValentContactStorePrivate *priv = valent_contact_store_get_instance_private (store);
  EContact *contact = NULL;

  g_assert (VALENT_IS_CONTACT_STORE (store));
  g_assert (uid != NULL);
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_assert (error == NULL || *error == NULL);

  if (!e_book_cache_get_contact (priv->cache, uid, FALSE, &contact, cancellable, error))
    return NULL;

  return contact;
}

static void
get_contact_task (GTask        *task,
                  gpointer      source_object,
                  gpointer      task_data,
                  GCancellable *cancellable)
{
  ValentContactStore *store = source_object;
  const char *uid = task_data;
  EContact *contact = NULL;
  GError *error = NULL;

  if (g_task_return_error_if_cancelled (task))
    return;

  if ((contact = valent_contact_store_get_contact (store, uid, cancellable, &error)) == NULL)
    return g_task_return_error (task, error);

  g_task_return_pointer (task, contact, g_object_unref);
}

static void
valent_contact_store_real_get_contact_async (ValentContactStore  *store,
                                             const char          *uid,
                                             GCancellable        *cancellable,
                                             GAsyncReadyCallback  callback,
                                             gpointer             user_data)
{
  g_autoptr (GTask) task = NULL;

  g_assert (VALENT_IS_CONTACT_STORE (store));
  g_assert (uid != NULL);
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (store, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_contact_store_real_get_contact_async);
  g_task_set_task_data (task, g_strdup (uid), g_free);
  g_task_run_in_thread (task, get_contact_task);
}

static GSList *
valent_contact_store_real_query (ValentContactStore  *store,
                                 const char          *query,
                                 GCancellable        *cancellable,
                                 GError             **error)
{
  ValentContactStorePrivate *priv = valent_contact_store_get_instance_private (store);
  GSList *results = NULL;
  GSList *contacts = NULL;

  g_assert (VALENT_IS_CONTACT_STORE (store));
  g_assert (query != NULL);
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_assert (error == NULL || *error == NULL);

  if (!e_book_cache_search (priv->cache, query, FALSE, &results, cancellable, error))
    return NULL;

  for (const GSList *iter = results; iter; iter = iter->next)
    {
      EBookCacheSearchData *result = iter->data;
      EContact *contact;

      contact = e_contact_new_from_vcard (result->vcard);
      contacts = g_slist_prepend (contacts, contact);
    }
  g_slist_free_full (results, e_book_cache_search_data_free);

  return g_slist_reverse (contacts);
}

static void
query_task (GTask        *task,
            gpointer      source_object,
            gpointer      task_data,
            GCancellable *cancellable)
{
  ValentContactStore *store = source_object;
  const char *query = task_data;
  GSList *results = NULL;
  GError *error = NULL;

  if (g_task_return_error_if_cancelled (task))
    return;

  results = valent_contact_store_query (store, query, cancellable, &error);

  if (error != NULL)
    return g_task_return_error (task, error);

  g_task_return_pointer (task, results, valent_object_slist_free);
}

static void
valent_contact_store_real_query_async (ValentContactStore  *store,
                                       const char          *query,
                                       GCancellable        *cancellable,
                                       GAsyncReadyCallback  callback,
                                       gpointer             user_data)
{
  g_autoptr (GTask) task = NULL;

  g_assert (VALENT_IS_CONTACT_STORE (store));
  g_assert (query != NULL);
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (store, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_contact_store_real_query_async);
  g_task_set_task_data (task, g_strdup (query), g_free);
  g_task_run_in_thread (task, query_task);
}

static void
valent_contact_store_real_prepare_backend (ValentContactStore *store)
{
  ValentContactStorePrivate *priv = valent_contact_store_get_instance_private (store);
  g_autoptr (ValentData) data = NULL;
  g_autoptr (GError) error = NULL;
  g_autofree char *path = NULL;

  g_assert (priv->source != NULL);

  data = valent_data_new (e_source_get_uid (priv->source), NULL);
  path = g_build_filename (valent_data_get_cache_path (data),
                           "contacts.db",
                           NULL);
  priv->cache = e_book_cache_new (path, priv->source, NULL, &error);

  if (priv->cache == NULL)
    g_error ("[%s] %s", G_STRLOC, error->message);
}
/* LCOV_EXCL_STOP */

/*
 * GObject
 */
static void
valent_contact_store_constructed (GObject *object)
{
  ValentContactStore *store = VALENT_CONTACT_STORE (object);

  VALENT_CONTACT_STORE_GET_CLASS (store)->prepare_backend (store);

  G_OBJECT_CLASS (valent_contact_store_parent_class)->constructed (object);
}

static void
valent_contact_store_finalize (GObject *object)
{
  ValentContactStore *self = VALENT_CONTACT_STORE (object);
  ValentContactStorePrivate *priv = valent_contact_store_get_instance_private (self);

  g_clear_object (&priv->cache);
  g_clear_object (&priv->source);

  G_OBJECT_CLASS (valent_contact_store_parent_class)->finalize (object);
}

static void
valent_contact_store_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  ValentContactStore *self = VALENT_CONTACT_STORE (object);
  ValentContactStorePrivate *priv = valent_contact_store_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_NAME:
      g_value_set_string (value, valent_contact_store_get_name (self));
      break;

    case PROP_SOURCE:
      g_value_set_object (value, priv->source);
      break;

    case PROP_UID:
      g_value_set_string (value, valent_contact_store_get_uid (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_contact_store_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  ValentContactStore *self = VALENT_CONTACT_STORE (object);
  ValentContactStorePrivate *priv = valent_contact_store_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_NAME:
      valent_contact_store_set_name (self, g_value_get_string (value));
      break;

    case PROP_SOURCE:
      priv->source = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_contact_store_class_init (ValentContactStoreClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = valent_contact_store_constructed;
  object_class->finalize = valent_contact_store_finalize;
  object_class->get_property = valent_contact_store_get_property;
  object_class->set_property = valent_contact_store_set_property;

  klass->add = valent_contact_store_real_add;
  klass->add_async = valent_contact_store_real_add_async;
  klass->remove = valent_contact_store_real_remove;
  klass->remove_async = valent_contact_store_real_remove_async;
  klass->query = valent_contact_store_real_query;
  klass->query_async = valent_contact_store_real_query_async;
  klass->get_contact = valent_contact_store_real_get_contact;
  klass->get_contact_async = valent_contact_store_real_get_contact_async;
  klass->prepare_backend = valent_contact_store_real_prepare_backend;

  /**
   * ValentContactStore:name:
   *
   * The display name.
   */
  properties [PROP_NAME] =
    g_param_spec_string ("name",
                         "Name",
                         "The display name",
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentContactStore:source:
   *
   * The #ESource describing the store.
   */
  properties [PROP_SOURCE] =
    g_param_spec_object ("source",
                         "Source",
                         "The ESource backing the store",
                         E_TYPE_SOURCE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentContactStore:uid:
   *
   * The unique identifier.
   */
  properties [PROP_UID] =
    g_param_spec_string ("uid",
                         "UID",
                         "The unique identifier",
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);

  /**
   * ValentContactStore::contact-added:
   * @store: a #ValentContactStore
   * @uid: the UID of @contact
   * @contact: the #EContact
   *
   * ValentContactStore::contact-added is emitted when a new contact is added to
   * @store.
   */
  signals [CONTACT_ADDED] =
    g_signal_new ("contact-added",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ValentContactStoreClass, contact_added),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  2,
                  G_TYPE_STRING,
                  E_TYPE_CONTACT);

  /**
   * ValentContactStore::contact-removed:
   * @store: a #ValentContactStore
   * @uid: the UID of @contact
   * @contact: the #EContact
   *
   * ValentContactStore::contact-added is emitted when a contact is removed from
   * @store.
   */
  signals [CONTACT_REMOVED] =
    g_signal_new ("contact-removed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ValentContactStoreClass, contact_removed),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  2,
                  G_TYPE_STRING,
                  E_TYPE_CONTACT);
}

static void
valent_contact_store_init (ValentContactStore *store)
{
}

/**
 * valent_contact_store_emit_contact_added:
 * @store: a #ValentContactStore
 * @uid: the UID of @contact
 * @contact: the #EContact
 *
 * Emits the #ValentContacttModel::contact-added signal on @list.
 *
 * This function should only be called by classes implementing
 * #ValentContactStore. It has to be called after the internal representation
 * of @store has been updated, because handlers connected to this signal
 * might query the new state of the provider.
 */
void
valent_contact_store_emit_contact_added (ValentContactStore *store,
                                         const char         *uid,
                                         EContact           *contact)
{
  ChangeEmission *emission;

  g_return_if_fail (VALENT_IS_CONTACT_STORE (store));

  if G_LIKELY (VALENT_IS_MAIN_THREAD ())
    {
      g_signal_emit (G_OBJECT (store), signals [CONTACT_ADDED], 0, uid, contact);
      return;
    }

  emission = g_new0 (ChangeEmission, 1);
  emission->store = g_object_ref (store);
  emission->uid = g_strdup (uid);
  emission->contact = g_object_ref (contact);
  emission->signal_id = signals [CONTACT_ADDED];

  g_timeout_add (0, emit_change_main, g_steal_pointer (&emission));
}

/**
 * valent_contact_store_emit_contact_removed:
 * @store: a #ValentContactStore
 * @uid: the UID of @contact
 * @contact: the #EContact
 *
 * Emits the #ValentContacttModel::contact-removed signal on @list.
 *
 * This function should only be called by classes implementing
 * #ValentContactStore. It has to be called after the internal representation
 * of @store has been updated, because handlers connected to this signal
 * might query the new state of the provider.
 */
void
valent_contact_store_emit_contact_removed (ValentContactStore *store,
                                           const char         *uid,
                                           EContact           *contact)
{
  ChangeEmission *emission;

  g_return_if_fail (VALENT_IS_CONTACT_STORE (store));

  if G_LIKELY (VALENT_IS_MAIN_THREAD ())
    {
      g_signal_emit (G_OBJECT (store), signals [CONTACT_REMOVED], 0, uid, contact);
      return;
    }

  emission = g_new0 (ChangeEmission, 1);
  emission->store = g_object_ref (store);
  emission->uid = g_strdup (uid);
  if (contact)
    emission->contact = g_object_ref (contact);
  emission->signal_id = signals [CONTACT_REMOVED];

  g_timeout_add (0, emit_change_main, g_steal_pointer (&emission));
}

/**
 * valent_contact_store_get_name:
 * @store: a #ValentContactStore
 *
 * Get the display name of @store.
 *
 * Returns: (transfer none): a display name
 */
const char *
valent_contact_store_get_name (ValentContactStore *store)
{
  ValentContactStorePrivate *priv = valent_contact_store_get_instance_private (store);

  g_return_val_if_fail (VALENT_IS_CONTACT_STORE (store), NULL);

  return e_source_get_display_name (priv->source);
}

/**
 * valent_contact_store_set_name:
 * @store: a #ValentContactStore
 * @name: a display name
 *
 * Set the display name of @store to @name.
 */
void
valent_contact_store_set_name (ValentContactStore *store,
                               const char         *name)
{
  ValentContactStorePrivate *priv = valent_contact_store_get_instance_private (store);

  g_return_if_fail (VALENT_IS_CONTACT_STORE (store));
  g_return_if_fail (name != NULL);

  if (priv->source != NULL)
    e_source_set_display_name (priv->source, name);
}

/**
 * valent_contact_store_get_source:
 * @store: a #ValentContactStore
 *
 * Get the #ESource backing @store.
 *
 * Returns: (transfer none): an #ESource
 */
ESource *
valent_contact_store_get_source (ValentContactStore *store)
{
  ValentContactStorePrivate *priv = valent_contact_store_get_instance_private (store);

  g_return_val_if_fail (VALENT_IS_CONTACT_STORE (store), NULL);

  return priv->source;
}

/**
 * valent_contact_store_get_uid:
 * @store: a #ValentContactStore
 *
 * Get the UID of @store.
 *
 * Returns: (transfer none): a UID
 */
const char *
valent_contact_store_get_uid (ValentContactStore *store)
{
  ValentContactStorePrivate *priv = valent_contact_store_get_instance_private (store);

  g_return_val_if_fail (VALENT_IS_CONTACT_STORE (store), NULL);

  return e_source_get_uid (priv->source);
}

/**
 * valent_contact_store_add: (virtual add)
 * @store: a #ValentContactStore
 * @contact: an #EContact
 * @cancellable: (nullable): #GCancellable
 * @error: (nullable): a #GError
 *
 * Add @contact to @store. This is optional and should only be implemented by
 * writable address books.
 *
 * Returns: %TRUE, or %FALSE with @error set
 */
gboolean
valent_contact_store_add (ValentContactStore  *store,
                          EContact            *contact,
                          GCancellable        *cancellable,
                          GError             **error)
{
  gboolean ret;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_CONTACT_STORE (store), FALSE);
  g_return_val_if_fail (E_IS_CONTACT (contact), FALSE);
  g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  ret = VALENT_CONTACT_STORE_GET_CLASS (store)->add (store, contact, cancellable, error);

  VALENT_RETURN (ret);
}

/**
 * valent_contact_store_add_async: (virtual add_async)
 * @store: a #ValentContactStore
 * @contact: a #EContact
 * @cancellable: (nullable): #GCancellable
 * @callback: (scope async): a #GAsyncReadyCallback
 * @user_data: (closure): user supplied data
 *
 * Asynchronous version of valent_contact_store_add(). Call
 * valent_contact_store_add_finish() to get the result.
 */
void
valent_contact_store_add_async (ValentContactStore  *store,
                                EContact            *contact,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_CONTACT_STORE (store));
  g_return_if_fail (E_IS_CONTACT (contact));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  VALENT_CONTACT_STORE_GET_CLASS (store)->add_async (store,
                                                     contact,
                                                     cancellable,
                                                     callback,
                                                     user_data);

  VALENT_EXIT;
}

/**
 * valent_contact_store_add_finish:
 * @store: a #ValentContactStore
 * @result: a #GAsyncResult
 * @error: (nullable): a #GError
 *
 * Finish an operation started by valent_contact_store_add_async().
 *
 * Returns: %TRUE, or %FALSE with @error set
 */
gboolean
valent_contact_store_add_finish (ValentContactStore  *store,
                                 GAsyncResult        *result,
                                 GError             **error)
{
  gboolean ret;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_CONTACT_STORE (store), FALSE);
  g_return_val_if_fail (g_task_is_valid (result, store), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  ret = g_task_propagate_boolean (G_TASK (result), error);

  VALENT_RETURN (ret);
}

/**
 * valent_contact_store_remove: (virtual remove)
 * @store: a #ValentContactStore
 * @uid: contact id
 * @cancellable: (nullable): #GCancellable
 * @error: (nullable): a #GError
 *
 * Remove the @contact from the address book.
 *
 * Returns: %TRUE, or %FALSE with @error set
 */
gboolean
valent_contact_store_remove (ValentContactStore  *store,
                             const char          *uid,
                             GCancellable        *cancellable,
                             GError             **error)
{
  gboolean ret;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_CONTACT_STORE (store), FALSE);
  g_return_val_if_fail (uid != NULL, FALSE);
  g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  ret = VALENT_CONTACT_STORE_GET_CLASS (store)->remove (store, uid, cancellable, error);

  VALENT_RETURN (ret);
}

/**
 * valent_contact_store_remove_async: (virtual remove_async)
 * @store: a #ValentContactStore
 * @uid: a contact id
 * @cancellable: (nullable): #GCancellable
 * @callback: (scope async): a #GAsyncReadyCallback
 * @user_data: (closure): user supplied data
 *
 * Asynchronous version of valent_contact_store_remove(). Call
 * valent_contact_store_remove_finish() to get the result.
 */
void
valent_contact_store_remove_async (ValentContactStore  *store,
                                   const char          *uid,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_CONTACT_STORE (store));
  g_return_if_fail (uid != NULL);
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  VALENT_CONTACT_STORE_GET_CLASS (store)->remove_async (store,
                                                        uid,
                                                        cancellable,
                                                        callback,
                                                        user_data);

  VALENT_EXIT;
}

/**
 * valent_contact_store_remove_finish:
 * @store: a #ValentContactStore
 * @result: a #GAsyncResult
 * @error: (nullable): a #GError
 *
 * Finish an operation started by valent_contact_store_remove_async().
 *
 * Returns: %TRUE, or %FALSE with @error set
 */
gboolean
valent_contact_store_remove_finish (ValentContactStore  *store,
                                    GAsyncResult        *result,
                                    GError             **error)
{
  gboolean ret;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_CONTACT_STORE (store), FALSE);
  g_return_val_if_fail (g_task_is_valid (result, store), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  ret = g_task_propagate_boolean (G_TASK (result), error);

  VALENT_RETURN (ret);
}

/**
 * valent_contact_store_query: (virtual query)
 * @store: a #ValentContactStore
 * @query: a search expression
 * @cancellable: (nullable): #GCancellable
 * @error: (nullable): a #GError
 *
 * Search @store for contacts matching @query.
 *
 * Returns: (transfer full) (element-type EContact): a #GSList
 */
GSList *
valent_contact_store_query (ValentContactStore  *store,
                            const char          *query,
                            GCancellable        *cancellable,
                            GError             **error)
{
  GSList *ret;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_CONTACT_STORE (store), NULL);
  g_return_val_if_fail (query != NULL, NULL);
  g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  ret = VALENT_CONTACT_STORE_GET_CLASS (store)->query (store, query, cancellable, error);

  VALENT_RETURN (ret);
}

/**
 * valent_contact_store_query_async: (virtual query_async)
 * @store: a #ValentContactStore
 * @query: a search expression
 * @cancellable: (nullable): #GCancellable
 * @callback: (scope async): a #GAsyncReadyCallback
 * @user_data: (closure): user supplied data
 *
 * Asynchronous version of valent_contact_store_query(). Call
 * valent_contact_store_query_finish() to get the result.
 */
void
valent_contact_store_query_async (ValentContactStore  *store,
                                  const char          *query,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_CONTACT_STORE (store));
  g_return_if_fail (query != NULL);
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  VALENT_CONTACT_STORE_GET_CLASS (store)->query_async (store,
                                                       query,
                                                       cancellable,
                                                       callback,
                                                       user_data);

  VALENT_EXIT;
}

/**
 * valent_contact_store_query_finish:
 * @store: a #ValentContactStore
 * @result: a #GAsyncResult
 * @error: (nullable): a #GError
 *
 * Finish an operation started by valent_contact_store_query_async().
 *
 * Returns: (transfer full) (element-type EContact): a #GSList
 */
GSList *
valent_contact_store_query_finish (ValentContactStore  *store,
                                   GAsyncResult        *result,
                                   GError             **error)
{
  GSList *ret;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_CONTACT_STORE (store), NULL);
  g_return_val_if_fail (g_task_is_valid (result, store), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  ret = g_task_propagate_pointer (G_TASK (result), error);

  VALENT_RETURN (ret);
}

/**
 * valent_contact_store_get_contact: (virtual get_contact)
 * @store: a #ValentContactStore
 * @uid: a unique identifier
 * @cancellable: (nullable): #GCancellable
 * @error: (nullable): a #GError
 *
 * Get the #EContact in @store by @uid, or %NULL if it doesn't exist.
 *
 * Returns: (transfer full) (nullable): a #EContact
 */
EContact *
valent_contact_store_get_contact (ValentContactStore  *store,
                                  const char          *uid,
                                  GCancellable        *cancellable,
                                  GError             **error)
{
  EContact *ret;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_CONTACT_STORE (store), NULL);
  g_return_val_if_fail (uid != NULL, NULL);
  g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  ret = VALENT_CONTACT_STORE_GET_CLASS (store)->get_contact (store, uid, cancellable, error);

  VALENT_RETURN (ret);
}

/**
 * valent_contact_store_get_contact_async: (virtual get_contact_async)
 * @store: a #ValentContactStore
 * @uid: a contact UID
 * @cancellable: (nullable): #GCancellable
 * @callback: (scope async): a #GAsyncReadyCallback
 * @user_data: (closure): user supplied data
 *
 * Asynchronous version of valent_contact_store_get_contact(). Call
 * valent_contact_store_get_contact_finish() to get the result.
 */
void
valent_contact_store_get_contact_async (ValentContactStore  *store,
                                        const char          *uid,
                                        GCancellable        *cancellable,
                                        GAsyncReadyCallback  callback,
                                        gpointer             user_data)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_CONTACT_STORE (store));
  g_return_if_fail (uid != NULL);
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  VALENT_CONTACT_STORE_GET_CLASS (store)->get_contact_async (store,
                                                             uid,
                                                             cancellable,
                                                             callback,
                                                             user_data);

  VALENT_EXIT;
}

/**
 * valent_contact_store_get_contact_finish:
 * @store: a #ValentContactStore
 * @result: a #GAsyncResult
 * @error: (nullable): a #GError
 *
 * Finish an operation started by valent_contact_store_get_contact_async().
 *
 * Returns: (transfer full) (nullable): a #EContact
 */
EContact *
valent_contact_store_get_contact_finish (ValentContactStore  *store,
                                         GAsyncResult        *result,
                                         GError             **error)
{
  EContact *ret;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_CONTACT_STORE (store), NULL);
  g_return_val_if_fail (g_task_is_valid (result, store), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  ret = g_task_propagate_pointer (G_TASK (result), error);

  VALENT_RETURN (ret);
}

/**
 * valent_contact_store_get_contacts_async:
 * @store: a #ValentContactStore
 * @uids: a list of UIDs
 * @cancellable: (nullable): #GCancellable
 * @callback: (scope async): a #GAsyncReadyCallback
 * @user_data: (closure): user supplied data
 *
 * A convenience wrapper around valent_contact_store_query_async() for searching contacts by UID.
 * Call valent_contact_store_query_finish() to get the result.
 */
void
valent_contact_store_get_contacts_async (ValentContactStore   *store,
                                         char                **uids,
                                         GCancellable         *cancellable,
                                         GAsyncReadyCallback   callback,
                                         gpointer              user_data)
{
  g_autofree char *sexp = NULL;
  g_autofree EBookQuery **queries = NULL;
  g_autoptr (EBookQuery) query = NULL;
  unsigned int n;

  g_return_if_fail (VALENT_IS_CONTACT_STORE (store));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  n = g_strv_length (uids);
  queries = g_new0 (EBookQuery *, n);

  for (unsigned int i = 0; i < n; i++)
    queries[i] = e_book_query_field_test (E_CONTACT_UID, E_BOOK_QUERY_IS, uids[i]);

  query = e_book_query_or (n, queries, TRUE);
  sexp = e_book_query_to_string (query);

  valent_contact_store_query_async (store, sexp, cancellable, callback, user_data);
}

/**
 * valent_contact_store_dup_for_phone:
 * @store: a #ValentContactStore
 * @number: a phone number string
 *
 * Return a copy of the first #EContact in @store with @phone. If it does not
 * exist a new #EContact will be made.
 *
 * This is a convenience wrapper around valent_contact_store_query() that perfers
 * libphonenumber, but falls back to iterating the full list of contacts.
 *
 * Returns: (transfer full): a #EContact
 */
EContact *
valent_contact_store_dup_for_phone (ValentContactStore *store,
                                    const char         *number)
{
  g_autoslist (GObject) contacts = NULL;
  g_autoptr (EBookQuery) query = NULL;
  g_autofree char *sexp = NULL;
  EContact *contact = NULL;

  g_return_val_if_fail (VALENT_IS_CONTACT_STORE (store), NULL);
  g_return_val_if_fail (number != NULL, NULL);

  /* Prefer using libphonenumber */
  if (e_phone_number_is_supported ())
    {
      query = e_book_query_field_test (E_CONTACT_TEL,
                                       E_BOOK_QUERY_EQUALS_SHORT_PHONE_NUMBER,
                                       number);
      sexp = e_book_query_to_string (query);

      contacts = valent_contact_store_query (store, sexp, NULL, NULL);

      if (contacts != NULL)
        contact = g_object_ref (contacts->data);
    }
  else
    {
      g_autofree char *normalized = NULL;

      query = e_book_query_field_exists (E_CONTACT_TEL);
      sexp = e_book_query_to_string (query);

      contacts = valent_contact_store_query (store, sexp, NULL, NULL);

      normalized = valent_phone_number_normalize (number);

      for (const GSList *iter = contacts; iter; iter = iter->next)
        {
          if (valent_phone_number_of_contact (iter->data, normalized))
            {
              contact = g_object_ref (iter->data);
              break;
            }
        }
    }

  /* Return a dummy contact on failure */
  if (contact == NULL)
    {
      contact = e_contact_new ();
      e_contact_set (contact, E_CONTACT_FULL_NAME, number);
      e_contact_set (contact, E_CONTACT_PHONE_OTHER, number);
    }

  return contact;
}

static void
dup_for_phone_cb (ValentContactStore *store,
                  GAsyncResult       *result,
                  gpointer            user_data)
{
  g_autoptr (GTask) task = user_data;
  const char *number = g_task_get_task_data (task);
  g_autoslist (GObject) contacts = NULL;
  EContact *contact = NULL;
  GError *error = NULL;

  contacts = valent_contact_store_query_finish (store, result, &error);

  if (error != NULL)
    return g_task_return_error (task, error);

  /* Prefer using libphonenumber */
  if (e_phone_number_is_supported ())
    {
      if (contacts != NULL)
        contact = g_object_ref (contacts->data);
    }
  else
    {
      g_autofree char *normalized = NULL;

      normalized = valent_phone_number_normalize (number);

      for (const GSList *iter = contacts; iter; iter = iter->next)
        {
          if (valent_phone_number_of_contact (iter->data, normalized))
            {
              contact = g_object_ref (iter->data);
              break;
            }
        }
    }

  /* Return a dummy contact on failure */
  if (contact == NULL)
    {
      contact = e_contact_new ();
      e_contact_set (contact, E_CONTACT_FULL_NAME, number);
      e_contact_set (contact, E_CONTACT_PHONE_OTHER, number);
    }

  g_task_return_pointer (task, contact, g_object_unref);
}

/**
 * valent_contact_store_dup_for_phone_async:
 * @store: a #ValentContactStore
 * @number: a phone number string
 * @cancellable: (nullable): #GCancellable
 * @callback: (scope async): a #GAsyncReadyCallback
 * @user_data: (closure): user supplied data
 *
 * Asynchronous version of valent_contact_store_dup_for_phone(). Call
 * valent_contact_store_dup_for_phone_finish() to get the result.
 */
void
valent_contact_store_dup_for_phone_async (ValentContactStore  *store,
                                          const char          *number,
                                          GCancellable        *cancellable,
                                          GAsyncReadyCallback  callback,
                                          gpointer             user_data)
{
  g_autoptr (GTask) task = NULL;
  g_autoptr (EBookQuery) query = NULL;
  g_autofree char *sexp = NULL;

  g_return_if_fail (VALENT_IS_CONTACT_STORE (store));
  g_return_if_fail (number != NULL);
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (store, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_contact_store_dup_for_phone_async);
  g_task_set_task_data (task, g_strdup (number), g_free);

  /* Prefer using libphonenumber */
  if (e_phone_number_is_supported ())
    {
      query = e_book_query_field_test (E_CONTACT_TEL,
                                       E_BOOK_QUERY_EQUALS_SHORT_PHONE_NUMBER,
                                       number);
      sexp = e_book_query_to_string (query);
    }
  else
    {
      query = e_book_query_field_exists (E_CONTACT_TEL);
      sexp = e_book_query_to_string (query);
    }

  valent_contact_store_query_async (store,
                                    sexp,
                                    cancellable,
                                    (GAsyncReadyCallback)dup_for_phone_cb,
                                    g_steal_pointer (&task));
}

/**
 * valent_contact_store_dup_for_phone_finish:
 * @store: a #ValentContactStore
 * @result: a #GAsyncResult
 * @error: (nullable): a #GError
 *
 * Finish an operation started by valent_contact_store_dup_contacts_async().
 *
 * Returns: (transfer full): an #EContact
 */
EContact *
valent_contact_store_dup_for_phone_finish (ValentContactStore  *store,
                                           GAsyncResult        *result,
                                           GError             **error)
{
  g_return_val_if_fail (VALENT_IS_CONTACT_STORE (store), NULL);
  g_return_val_if_fail (g_task_is_valid (result, store), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}

