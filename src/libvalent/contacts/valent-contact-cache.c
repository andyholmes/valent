// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-contact-cache"

#include "config.h"

#include <libvalent-core.h>

#include "valent-contact-cache.h"
#include "valent-contact-store.h"
#include "valent-contact-utils.h"
#include "valent-eds.h"


/**
 * SECTION:valentcontactcache
 * @short_description: An #EBookCache wrapper
 * @title: ValentContactCache
 * @stability: Unstable
 * @include: libvalent-contacts.h
 *
 * The #ValentContactCache class is an implementation of #ValentContactStore for
 * local contact stores. It is effectively a simple wrapper around #EBookCache,
 * providing a fallback when Evolution Data Server is not available.
 */

struct _ValentContactCache
{
  ValentContactStore  parent_instance;

  EBookCache         *cache;
  char               *path;
};

G_DEFINE_TYPE (ValentContactCache, valent_contact_cache, VALENT_TYPE_CONTACT_STORE)


enum {
  PROP_0,
  PROP_PATH,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };


/*
 * ValentContactStore
 */
static void
valent_contact_cache_add_task (GTask        *task,
                               gpointer      source_object,
                               gpointer      task_data,
                               GCancellable *cancellable)
{
  ValentContactCache *self = VALENT_CONTACT_CACHE (source_object);
  ValentContactStore *store = VALENT_CONTACT_STORE (source_object);
  GSList *contacts = task_data;
  GError *error = NULL;

  if (g_task_return_error_if_cancelled (task))
    return;

  valent_object_lock (VALENT_OBJECT (self));
  e_book_cache_put_contacts (self->cache,
                             contacts,
                             NULL,
                             NULL,
                             E_CACHE_IS_OFFLINE,
                             cancellable,
                             &error);
  valent_object_unlock (VALENT_OBJECT (self));

  if (error != NULL)
    return g_task_return_error (task, error);

  for (const GSList *iter = contacts; iter; iter = iter->next)
    valent_contact_store_emit_contact_added (store, E_CONTACT (iter->data));

  g_task_return_boolean (task, TRUE);
}

static void
valent_contact_cache_add_contacts (ValentContactStore  *store,
                                   GSList              *contacts,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data)
{
  g_autoptr (GTask) task = NULL;
  GSList *additions = NULL;

  g_assert (VALENT_IS_CONTACT_STORE (store));
  g_assert (contacts != NULL);
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  for (const GSList *iter = contacts; iter; iter = iter->next)
    additions = g_slist_append (additions, g_object_ref (iter->data));

  task = g_task_new (store, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_contact_cache_add_contacts);
  g_task_set_task_data (task, additions, valent_object_slist_free);
  g_task_run_in_thread (task, valent_contact_cache_add_task);
}

static void
valent_contact_cache_remove_task (GTask        *task,
                                  gpointer      source_object,
                                  gpointer      task_data,
                                  GCancellable *cancellable)
{
  ValentContactCache *self = VALENT_CONTACT_CACHE (source_object);
  ValentContactStore *store = VALENT_CONTACT_STORE (source_object);
  const char *uid = task_data;
  GError *error = NULL;

  if (g_task_return_error_if_cancelled (task))
    return;

  valent_object_lock (VALENT_OBJECT (self));
  e_book_cache_remove_contact (self->cache,
                               uid,
                               0,
                               E_CACHE_IS_OFFLINE,
                               cancellable,
                               &error);
  valent_object_unlock (VALENT_OBJECT (self));

  if (error != NULL)
    return g_task_return_error (task, error);

  valent_contact_store_emit_contact_removed (store, uid);

  g_task_return_boolean (task, TRUE);
}

static void
valent_contact_cache_remove_contact (ValentContactStore  *store,
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
  g_task_set_source_tag (task, valent_contact_cache_remove_contact);
  g_task_set_task_data (task, g_strdup (uid), g_free);
  g_task_run_in_thread (task, valent_contact_cache_remove_task);
}

static void
valent_contact_cache_get_contact_task (GTask        *task,
                                       gpointer      source_object,
                                       gpointer      task_data,
                                       GCancellable *cancellable)
{
  ValentContactCache *self = VALENT_CONTACT_CACHE (source_object);
  const char *uid = task_data;
  EContact *contact = NULL;
  GError *error = NULL;

  if (g_task_return_error_if_cancelled (task))
    return;

  valent_object_lock (VALENT_OBJECT (self));
  e_book_cache_get_contact (self->cache,
                            uid,
                            FALSE,
                            &contact,
                            cancellable,
                            &error);
  valent_object_unlock (VALENT_OBJECT (self));

  if (error != NULL)
    return g_task_return_error (task, error);

  g_task_return_pointer (task, contact, g_object_unref);
}

static void
valent_contact_cache_get_contact (ValentContactStore  *store,
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
  g_task_set_source_tag (task, valent_contact_cache_get_contact);
  g_task_set_task_data (task, g_strdup (uid), g_free);
  g_task_run_in_thread (task, valent_contact_cache_get_contact_task);
}

static void
valent_contact_cache_query_task (GTask        *task,
                                 gpointer      source_object,
                                 gpointer      task_data,
                                 GCancellable *cancellable)
{
  ValentContactCache *self = VALENT_CONTACT_CACHE (source_object);
  const char *query = task_data;
  GSList *results = NULL;
  GSList *contacts = NULL;
  GError *error = NULL;

  if (g_task_return_error_if_cancelled (task))
    return;

  valent_object_lock (VALENT_OBJECT (self));
  e_book_cache_search (self->cache,
                       query,
                       FALSE,
                       &results,
                       cancellable,
                       &error);
  valent_object_unlock (VALENT_OBJECT (self));

  if (error != NULL)
    return g_task_return_error (task, error);

  for (const GSList *iter = results; iter; iter = iter->next)
    {
      EBookCacheSearchData *result = iter->data;
      EContact *contact;

      contact = e_contact_new_from_vcard (result->vcard);
      contacts = g_slist_prepend (contacts, contact);
    }
  g_slist_free_full (results, e_book_cache_search_data_free);

  g_task_return_pointer (task, contacts, valent_object_slist_free);
}

static void
valent_contact_cache_query (ValentContactStore  *store,
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
  g_task_set_source_tag (task, valent_contact_cache_query);
  g_task_set_task_data (task, g_strdup (query), g_free);
  g_task_run_in_thread (task, valent_contact_cache_query_task);
}

/*
 * GObject
 */
static void
valent_contact_cache_constructed (GObject *object)
{
  ValentContactCache *self = VALENT_CONTACT_CACHE (object);
  ValentContactStore *store = VALENT_CONTACT_STORE (object);
  ESource *source;
  g_autoptr (ValentData) data = NULL;
  g_autoptr (GError) error = NULL;

  source = valent_contact_store_get_source (store);

  /* This will usually be the path for the contacts plugin, since the device ID
   * is used as the ESource UID. */
  if (self->path == NULL)
    {
      data = valent_data_new (e_source_get_uid (source), NULL);
      self->path = g_build_filename (valent_data_get_cache_path (data),
                                     "contacts",
                                     "contacts.db",
                                     NULL);
    }

  valent_object_lock (VALENT_OBJECT (self));
  self->cache = e_book_cache_new (self->path, source, NULL, &error);
  valent_object_unlock (VALENT_OBJECT (self));

  if (error != NULL)
    g_critical ("%s(): %s", G_STRFUNC, error->message);

  G_OBJECT_CLASS (valent_contact_cache_parent_class)->constructed (object);
}

static void
valent_contact_cache_finalize (GObject *object)
{
  ValentContactCache *self = VALENT_CONTACT_CACHE (object);

  valent_object_lock (VALENT_OBJECT (self));
  g_clear_object (&self->cache);
  g_clear_pointer (&self->path, g_free);
  valent_object_unlock (VALENT_OBJECT (self));

  G_OBJECT_CLASS (valent_contact_cache_parent_class)->finalize (object);
}

static void
valent_contact_cache_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  ValentContactCache *self = VALENT_CONTACT_CACHE (object);

  switch (prop_id)
    {
    case PROP_PATH:
      g_value_set_string (value, self->path);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_contact_cache_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  ValentContactCache *self = VALENT_CONTACT_CACHE (object);

  switch (prop_id)
    {
    case PROP_PATH:
      self->path = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_contact_cache_class_init (ValentContactCacheClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentContactStoreClass *store_class = VALENT_CONTACT_STORE_CLASS (klass);

  object_class->constructed = valent_contact_cache_constructed;
  object_class->finalize = valent_contact_cache_finalize;
  object_class->get_property = valent_contact_cache_get_property;
  object_class->set_property = valent_contact_cache_set_property;

  store_class->add_contacts = valent_contact_cache_add_contacts;
  store_class->remove_contact = valent_contact_cache_remove_contact;
  store_class->query = valent_contact_cache_query;
  store_class->get_contact = valent_contact_cache_get_contact;

  /**
   * ValentContactCache:path:
   *
   * The path to the database file.
   */
  properties [PROP_PATH] =
    g_param_spec_string ("path",
                         "Path",
                         "The path to the database file",
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
valent_contact_cache_init (ValentContactCache *self)
{
}

