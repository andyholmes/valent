// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-ebook-provider"

#include "config.h"

#include <gio/gio.h>
#include <libvalent-core.h>
#include <libvalent-contacts.h>

#include "valent-ebook-provider.h"
#include "valent-ebook-store.h"
#include "valent-eds-utils.h"


struct _ValentEBookProvider
{
  ValentContactStoreProvider  parent_instance;

  ESourceRegistry            *registry;
  GHashTable                 *stores;
};


G_DEFINE_TYPE (ValentEBookProvider, valent_ebook_provider, VALENT_TYPE_CONTACT_STORE_PROVIDER)


/*
 * ESourceRegistry Callbacks
 */
static void
on_source_added (ESourceRegistry     *registry,
                 ESource             *source,
                 ValentEBookProvider *self)
{
  ValentContactStoreProvider *provider = VALENT_CONTACT_STORE_PROVIDER (self);
  ValentContactStore *store;

  g_assert (E_IS_SOURCE_REGISTRY (registry));
  g_assert (E_IS_SOURCE (source));
  g_assert (VALENT_IS_EBOOK_PROVIDER (self));

  store = g_object_new (VALENT_TYPE_EBOOK_STORE,
                        "source", source,
                        NULL);
  g_hash_table_replace (self->stores, e_source_dup_uid (source), store);

  valent_contact_store_provider_emit_store_added (provider, store);
}

static void
on_source_removed (ESourceRegistry     *registry,
                   ESource             *source,
                   ValentEBookProvider *self)
{
  ValentContactStoreProvider *provider = VALENT_CONTACT_STORE_PROVIDER (self);
  gpointer uid, store;

  g_assert (E_IS_SOURCE_REGISTRY (registry));
  g_assert (E_IS_SOURCE (source));
  g_assert (VALENT_IS_EBOOK_PROVIDER (self));

  if (!g_hash_table_steal_extended (self->stores, e_source_get_uid (source), &uid, &store))
    {
      valent_contact_store_provider_emit_store_removed (provider, store);
      g_free (uid);
      g_object_unref (store);
    }
}

/*
 * ValentContactStoreProvider
 */
static gboolean
valent_ebook_provider_register (ValentContactStoreProvider  *provider,
                                ValentContactStore          *store,
                                GCancellable                *cancellable,
                                GError                     **error)
{
  ValentEBookProvider *self = VALENT_EBOOK_PROVIDER (provider);
  g_autoptr (ESource) scratch = NULL;
  g_autoptr (ESource) source = NULL;
  const char *uid;
  const char *name;

  g_assert (VALENT_IS_CONTACT_STORE_PROVIDER (provider));
  g_assert (VALENT_IS_CONTACT_STORE (store));

  uid = valent_contact_store_get_uid (store);
  name = valent_contact_store_get_name (store);

  /* Create and register a new source */
  scratch = valent_contacts_create_ebook_source (uid, name, NULL);

  if ((source = valent_eds_register_source (scratch, cancellable, error)) == NULL)
    return FALSE;

  return TRUE;
}

static void
load_task (GTask        *task,
           gpointer      source_object,
           gpointer      task_data,
           GCancellable *cancellable)
{
  ValentEBookProvider *self = source_object;
  GError *error = NULL;
  g_autolist (ESource) sources = NULL;

  g_assert (VALENT_IS_EBOOK_PROVIDER (self));

  self->registry = e_source_registry_new_sync (cancellable, &error);

  if (self->registry == NULL)
    return g_task_return_error (task, error);

  /* Load existing address books */
  sources = e_source_registry_list_sources (self->registry, E_SOURCE_EXTENSION_ADDRESS_BOOK);

  for (const GList *iter = sources; iter; iter = iter->next)
    on_source_added (self->registry, E_SOURCE (iter->data), self);

  g_signal_connect (self->registry,
                    "source-added",
                    G_CALLBACK (on_source_added),
                    self);
  g_signal_connect (self->registry,
                    "source-removed",
                    G_CALLBACK (on_source_removed),
                    self);

  /* g_autoptr (GPtrArray) stores = valent_contacts_get_stores (valent_contacts_get_default ()); */

  /* for (guint i = 0; i < stores->len; i++) */
  /*   { */
  /*     ValentContactStore *store = g_ptr_array_index (stores, i); */
  /*     g_autoptr (ESource) source = NULL; */

  /*     if (VALENT_IS_EBOOK_STORE (store)) */
  /*       continue; */

  /*     source = valent_eds_register_source (valent_contact_store_get_source (store), NULL, NULL); */
  /*   } */

  g_task_return_boolean (task, TRUE);
}

static void
valent_ebook_provider_load_async (ValentContactStoreProvider *provider,
                                  GCancellable               *cancellable,
                                  GAsyncReadyCallback         callback,
                                  gpointer                    user_data)
{
  g_autoptr (GTask) task = NULL;

  g_assert (VALENT_IS_EBOOK_PROVIDER (provider));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (provider, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_ebook_provider_load_async);
  g_task_run_in_thread (task, load_task);
}

/*
 * GObject
 */
static void
valent_ebook_provider_dispose (GObject *object)
{
  ValentEBookProvider *self = VALENT_EBOOK_PROVIDER (object);

  if (self->registry != NULL)
    {
      g_signal_handlers_disconnect_by_data (self->registry, self);
      g_clear_object (&self->registry);
    }

  g_hash_table_remove_all (self->stores);

  G_OBJECT_CLASS (valent_ebook_provider_parent_class)->dispose (object);
}

static void
valent_ebook_provider_finalize (GObject *object)
{
  ValentEBookProvider *self = VALENT_EBOOK_PROVIDER (object);

  g_clear_pointer (&self->stores, g_hash_table_unref);

  G_OBJECT_CLASS (valent_ebook_provider_parent_class)->finalize (object);
}

static void
valent_ebook_provider_class_init (ValentEBookProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentContactStoreProviderClass *provider_class = VALENT_CONTACT_STORE_PROVIDER_CLASS (klass);

  object_class->dispose = valent_ebook_provider_dispose;
  object_class->finalize = valent_ebook_provider_finalize;

  provider_class->load_async = valent_ebook_provider_load_async;
}

static void
valent_ebook_provider_init (ValentEBookProvider *self)
{
  self->stores = g_hash_table_new_full (g_str_hash, g_str_equal,
                                        g_free,     g_object_unref);
}

