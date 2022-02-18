// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-contact-store"

#include "config.h"

#include <libvalent-core.h>

#include "valent-contact-store.h"
#include "valent-contact-utils.h"
#include "valent-eds.h"


/**
 * SECTION:valentcontactstore
 * @short_description: A helper for ESource address books
 * @title: ValentContactStore
 * @stability: Unstable
 * @include: libvalent-contacts.h
 *
 * The #ValentContactStore class is a base class for objects providing #EContact
 * instances.
 *
 * If instantiated directly it is effectively a wrapper around #EBookCache,
 * providing a file-based fallback when Evolution Data Server is not available.
 *
 * If subclasses for another provider, all virtual function must be overridden.
 */

typedef struct
{
  ESource    *source;
} ValentContactStorePrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (ValentContactStore, valent_contact_store, VALENT_TYPE_OBJECT)

/**
 * ValentContactStoreClass:
 * @add_contacts: the virtual function pointer for valent_contact_store_add_contacts()
 * @get_contact: the virtual function pointer for valent_contact_store_get_contact()
 * @remove_contact: the virtual function pointer for valent_contact_store_remove_contact()
 * @query: the virtual function pointer for valent_contact_store_query()
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
 * Signal Emission Helpers
 */
typedef struct
{
  GRecMutex  mutex;
  GWeakRef   store;
  char      *uid;
  EContact  *contact;
} SignalEmission;

static gboolean
valent_contact_store_emit_contact_added_main (gpointer data)
{
  SignalEmission *emission = data;
  g_autoptr (ValentContactStore) store = NULL;

  g_assert (VALENT_IS_MAIN_THREAD ());

  g_rec_mutex_lock (&emission->mutex);
  if ((store = g_weak_ref_get (&emission->store)) != NULL)
    g_signal_emit (G_OBJECT (store),
                   signals [CONTACT_ADDED], 0,
                   emission->contact);

  g_weak_ref_clear (&emission->store);
  g_clear_object (&emission->contact);
  g_rec_mutex_unlock (&emission->mutex);
  g_rec_mutex_clear (&emission->mutex);
  g_clear_pointer (&emission, g_free);

  return G_SOURCE_REMOVE;
}

static gboolean
valent_contact_store_emit_contact_removed_main (gpointer data)
{
  SignalEmission *emission = data;
  g_autoptr (ValentContactStore) store = NULL;

  g_assert (VALENT_IS_MAIN_THREAD ());

  g_rec_mutex_lock (&emission->mutex);
  if ((store = g_weak_ref_get (&emission->store)) != NULL)
    g_signal_emit (G_OBJECT (store),
                   signals [CONTACT_REMOVED], 0,
                   emission->uid);

  g_weak_ref_clear (&emission->store);
  g_clear_pointer (&emission->uid, g_free);
  g_rec_mutex_unlock (&emission->mutex);
  g_rec_mutex_clear (&emission->mutex);
  g_clear_pointer (&emission, g_free);

  return G_SOURCE_REMOVE;
}

/* LCOV_EXCL_START */
static void
valent_contact_store_real_add_contacts (ValentContactStore  *store,
                                        GSList              *contacts,
                                        GCancellable        *cancellable,
                                        GAsyncReadyCallback  callback,
                                        gpointer             user_data)
{
  g_assert (VALENT_IS_CONTACT_STORE (store));
  g_assert (contacts != NULL);
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  g_task_report_new_error (store, callback, user_data,
                           valent_contact_store_real_add_contacts,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "%s does not implement add_contacts()",
                           G_OBJECT_TYPE_NAME (store));
}

static void
valent_contact_store_real_remove_contact (ValentContactStore  *store,
                                          const char          *uid,
                                          GCancellable        *cancellable,
                                          GAsyncReadyCallback  callback,
                                          gpointer             user_data)
{
  g_assert (VALENT_IS_CONTACT_STORE (store));
  g_assert (uid != NULL);
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  g_task_report_new_error (store, callback, user_data,
                           valent_contact_store_real_remove_contact,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "%s does not implement remove_contact()",
                           G_OBJECT_TYPE_NAME (store));
}

static void
valent_contact_store_real_get_contact (ValentContactStore  *store,
                                       const char          *uid,
                                       GCancellable        *cancellable,
                                       GAsyncReadyCallback  callback,
                                       gpointer             user_data)
{
  g_assert (VALENT_IS_CONTACT_STORE (store));
  g_assert (uid != NULL);
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  g_task_report_new_error (store, callback, user_data,
                           valent_contact_store_real_get_contact,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "%s does not implement get_contact()",
                           G_OBJECT_TYPE_NAME (store));
}

static void
valent_contact_store_real_query (ValentContactStore  *store,
                                 const char          *query,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
  g_assert (VALENT_IS_CONTACT_STORE (store));
  g_assert (query != NULL);
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  g_task_report_new_error (store, callback, user_data,
                           valent_contact_store_real_query,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "%s does not implement query()",
                           G_OBJECT_TYPE_NAME (store));
}
/* LCOV_EXCL_STOP */

/*
 * GObject
 */
static void
valent_contact_store_finalize (GObject *object)
{
  ValentContactStore *self = VALENT_CONTACT_STORE (object);
  ValentContactStorePrivate *priv = valent_contact_store_get_instance_private (self);

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

  object_class->finalize = valent_contact_store_finalize;
  object_class->get_property = valent_contact_store_get_property;
  object_class->set_property = valent_contact_store_set_property;

  klass->add_contacts = valent_contact_store_real_add_contacts;
  klass->remove_contact = valent_contact_store_real_remove_contact;
  klass->query = valent_contact_store_real_query;
  klass->get_contact = valent_contact_store_real_get_contact;

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
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE,
                  1,
                  E_TYPE_CONTACT);
  g_signal_set_va_marshaller (signals [CONTACT_ADDED],
                              G_TYPE_FROM_CLASS (klass),
                              g_cclosure_marshal_VOID__OBJECTv);

  /**
   * ValentContactStore::contact-removed:
   * @store: a #ValentContactStore
   * @uid: the UID of the removed contact
   *
   * ValentContactStore::contact-removed is emitted when a contact is removed
   * from @store.
   */
  signals [CONTACT_REMOVED] =
    g_signal_new ("contact-removed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ValentContactStoreClass, contact_removed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__STRING,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_STRING);
  g_signal_set_va_marshaller (signals [CONTACT_REMOVED],
                              G_TYPE_FROM_CLASS (klass),
                              g_cclosure_marshal_VOID__STRINGv);
}

static void
valent_contact_store_init (ValentContactStore *store)
{
}

/**
 * valent_contact_store_emit_contact_added:
 * @store: a #ValentContactStore
 * @contact: the #EContact
 *
 * Emits the #ValentContactStore::contact-added signal on @store.
 *
 * This function should only be called by classes implementing
 * #ValentContactStore. It has to be called after the internal representation
 * of @store has been updated, because handlers connected to this signal
 * might query the new state of the provider.
 */
void
valent_contact_store_emit_contact_added (ValentContactStore *store,
                                         EContact           *contact)
{
  SignalEmission *emission;

  g_return_if_fail (VALENT_IS_CONTACT_STORE (store));

  if G_LIKELY (VALENT_IS_MAIN_THREAD ())
    {
      g_signal_emit (G_OBJECT (store), signals [CONTACT_ADDED], 0, contact);
      return;
    }

  emission = g_new0 (SignalEmission, 1);
  g_rec_mutex_init (&emission->mutex);
  g_rec_mutex_lock (&emission->mutex);
  g_weak_ref_init (&emission->store, store);
  emission->contact = g_object_ref (contact);
  g_rec_mutex_unlock (&emission->mutex);

  g_timeout_add (0, valent_contact_store_emit_contact_added_main, emission);
}

/**
 * valent_contact_store_emit_contact_removed:
 * @store: a #ValentContactStore
 * @uid: the UID of @contact
 *
 * Emits the #ValentContactStore::contact-removed signal on @store.
 *
 * This function should only be called by classes implementing
 * #ValentContactStore. It has to be called after the internal representation
 * of @store has been updated, because handlers connected to this signal
 * might query the new state of the provider.
 */
void
valent_contact_store_emit_contact_removed (ValentContactStore *store,
                                           const char         *uid)
{
  SignalEmission *emission;

  g_return_if_fail (VALENT_IS_CONTACT_STORE (store));

  if G_LIKELY (VALENT_IS_MAIN_THREAD ())
    {
      g_signal_emit (G_OBJECT (store), signals [CONTACT_REMOVED], 0, uid);
      return;
    }

  emission = g_new0 (SignalEmission, 1);
  g_rec_mutex_init (&emission->mutex);
  g_rec_mutex_lock (&emission->mutex);
  g_weak_ref_init (&emission->store, store);
  emission->uid = g_strdup (uid);
  g_rec_mutex_unlock (&emission->mutex);

  g_timeout_add (0, valent_contact_store_emit_contact_removed_main, emission);
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
 * valent_contact_store_add_contact:
 * @store: a #ValentContactStore
 * @contact: a #EContact
 * @cancellable: (nullable): #GCancellable
 * @callback: (scope async): a #GAsyncReadyCallback
 * @user_data: (closure): user supplied data
 * This is a convenience wrapper for valent_contact_store_add_contacts(), which
 * is the preferred way to add or modify multiple contacts when possible.
 */
void
valent_contact_store_add_contact (ValentContactStore  *store,
                                  EContact            *contact,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data)
{
  g_autoptr (GSList) contacts = NULL;

  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_CONTACT_STORE (store));
  g_return_if_fail (E_IS_CONTACT (contact));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  contacts = g_slist_append (contacts, contact);
  valent_contact_store_add_contacts (store,
                                     contacts,
                                     cancellable,
                                     callback,
                                     user_data);

  VALENT_EXIT;
}

/**
 * valent_contact_store_add_contacts: (virtual add_contacts)
 * @store: a #ValentContactStore
 * @contacts: (element-type EContact): a #GSList
 * @cancellable: (nullable): #GCancellable
 * @callback: (scope async): a #GAsyncReadyCallback
 * @user_data: (closure): user supplied data
 *
 * Add @contacts to @store. Call valent_contact_store_add_finish() to get the
 * result.
 */
void
valent_contact_store_add_contacts (ValentContactStore  *store,
                                   GSList              *contacts,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_CONTACT_STORE (store));
  g_return_if_fail (contacts != NULL);
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  VALENT_CONTACT_STORE_GET_CLASS (store)->add_contacts (store,
                                                        contacts,
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
 * Finish an operation started by valent_contact_store_add_contact() or
 * valent_contact_store_add_contacts().
 *
 * Returns: %TRUE if successful, or %FALSE with @error set
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
 * valent_contact_store_remove_contact: (virtual remove_contact)
 * @store: a #ValentContactStore
 * @uid: a contact UID
 * @cancellable: (nullable): #GCancellable
 * @callback: (scope async): a #GAsyncReadyCallback
 * @user_data: (closure): user supplied data
 *
 * Remove contact @uid from @store. Call valent_contact_store_remove_finish() to
 * get the result.
 */
void
valent_contact_store_remove_contact (ValentContactStore  *store,
                                     const char          *uid,
                                     GCancellable        *cancellable,
                                     GAsyncReadyCallback  callback,
                                     gpointer             user_data)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_CONTACT_STORE (store));
  g_return_if_fail (uid != NULL);
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  VALENT_CONTACT_STORE_GET_CLASS (store)->remove_contact (store,
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
 * Finish an operation started by valent_contact_store_remove_contact().
 *
 * Returns: %TRUE if successful, or %FALSE with @error set
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
 * @callback: (scope async): a #GAsyncReadyCallback
 * @user_data: (closure): user supplied data
 *
 * Asynchronous version of valent_contact_store_query(). Call
 * valent_contact_store_query_finish() to get the result.
 */
void
valent_contact_store_query (ValentContactStore  *store,
                            const char          *query,
                            GCancellable        *cancellable,
                            GAsyncReadyCallback  callback,
                            gpointer             user_data)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_CONTACT_STORE (store));
  g_return_if_fail (query != NULL);
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  VALENT_CONTACT_STORE_GET_CLASS (store)->query (store,
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
 * Finish an operation started by valent_contact_store_query().
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
 * @uid: a contact UID
 * @cancellable: (nullable): #GCancellable
 * @callback: (scope async): a #GAsyncReadyCallback
 * @user_data: (closure): user supplied data
 *
 * Asynchronous version of valent_contact_store_get_contact(). Call
 * valent_contact_store_get_contact_finish() to get the result.
 */
void
valent_contact_store_get_contact (ValentContactStore  *store,
                                  const char          *uid,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_CONTACT_STORE (store));
  g_return_if_fail (uid != NULL);
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  VALENT_CONTACT_STORE_GET_CLASS (store)->get_contact (store,
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
 * Finish an operation started by valent_contact_store_get_contact().
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
 * valent_contact_store_get_contacts:
 * @store: a #ValentContactStore
 * @uids: a list of UIDs
 * @cancellable: (nullable): #GCancellable
 * @callback: (scope async): a #GAsyncReadyCallback
 * @user_data: (closure): user supplied data
 *
 * A convenience wrapper around valent_contact_store_query() for searching
 * contacts by UID. Call valent_contact_store_query_finish() to get the result.
 */
void
valent_contact_store_get_contacts (ValentContactStore   *store,
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

  valent_contact_store_query (store, sexp, cancellable, callback, user_data);
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

  valent_contact_store_query (store,
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

