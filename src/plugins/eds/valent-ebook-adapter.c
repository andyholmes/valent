// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-ebook-adapter"

#include "config.h"

#include <gio/gio.h>
#include <libvalent-core.h>
#include <libvalent-contacts.h>

#include "valent-ebook-adapter.h"
#include "valent-ebook-store.h"


struct _ValentEBookAdapter
{
  ValentContactsAdapter  parent_instance;

  GCancellable          *cancellable;
  ESourceRegistry       *registry;
  GHashTable            *stores;
};


G_DEFINE_TYPE (ValentEBookAdapter, valent_ebook_adapter, VALENT_TYPE_CONTACTS_ADAPTER)


/*
 * ESourceRegistry Callbacks
 */
static void
e_book_client_connect_cb (GObject            *object,
                          GAsyncResult       *result,
                          ValentEBookAdapter *self)
{
  g_autoptr (EClient) client = NULL;
  g_autoptr (GError) error = NULL;
  ValentContactStore *store;
  ESource *source;

  if ((client = e_book_client_connect_finish (result, &error)) == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Failed loading address book: %s", error->message);

      return;
    }

  source = e_client_get_source (client);
  store = g_object_new (VALENT_TYPE_EBOOK_STORE,
                        "client", client,
                        "source", source,
                        NULL);
  g_hash_table_replace (self->stores, g_object_ref (source), store);
  valent_contacts_adapter_emit_store_added (VALENT_CONTACTS_ADAPTER (self),
                                            store);
}

static void
on_source_added (ESourceRegistry    *registry,
                 ESource            *source,
                 ValentEBookAdapter *self)
{
  g_assert (E_IS_SOURCE_REGISTRY (registry));
  g_assert (E_IS_SOURCE (source));
  g_assert (VALENT_IS_EBOOK_ADAPTER (self));

  if (!e_source_has_extension (source, E_SOURCE_EXTENSION_ADDRESS_BOOK))
    return;

  e_book_client_connect (source,
                         -1,
                         self->cancellable,
                         (GAsyncReadyCallback)e_book_client_connect_cb,
                         self);
}

static void
on_source_removed (ESourceRegistry    *registry,
                   ESource            *source,
                   ValentEBookAdapter *self)
{
  ValentContactsAdapter *adapter = VALENT_CONTACTS_ADAPTER (self);
  gpointer esource, store;

  g_assert (E_IS_SOURCE_REGISTRY (registry));
  g_assert (E_IS_SOURCE (source));
  g_assert (VALENT_IS_EBOOK_ADAPTER (self));

  if (!e_source_has_extension (source, E_SOURCE_EXTENSION_ADDRESS_BOOK))
    return;

  if (g_hash_table_steal_extended (self->stores, source, &esource, &store))
    {
      valent_contacts_adapter_emit_store_removed (adapter, store);
      g_object_unref (esource);
      g_object_unref (store);
    }
}

/*
 * ValentContactsAdapter
 */
static void
e_source_registry_new_cb (GObject      *object,
                          GAsyncResult *result,
                          gpointer      user_data)
{
  g_autoptr (GTask) task = G_TASK (user_data);
  g_autolist (ESource) sources = NULL;
  ValentEBookAdapter *self = g_task_get_source_object (task);
  GError *error = NULL;

  g_assert (VALENT_IS_EBOOK_ADAPTER (self));

  if ((self->registry = e_source_registry_new_finish (result, &error)) == NULL)
    return g_task_return_error (task, error);

  /* Load existing address books */
  sources = e_source_registry_list_sources (self->registry,
                                            E_SOURCE_EXTENSION_ADDRESS_BOOK);

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

  g_task_return_boolean (task, TRUE);
}

static void
valent_ebook_adapter_load_async (ValentContactsAdapter *adapter,
                                 GCancellable          *cancellable,
                                 GAsyncReadyCallback    callback,
                                 gpointer               user_data)
{
  g_autoptr (GTask) task = NULL;

  g_assert (VALENT_IS_EBOOK_ADAPTER (adapter));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (adapter, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_ebook_adapter_load_async);

  e_source_registry_new (cancellable,
                         e_source_registry_new_cb,
                         g_steal_pointer (&task));
}

/*
 * GObject
 */
static void
valent_ebook_adapter_dispose (GObject *object)
{
  ValentEBookAdapter *self = VALENT_EBOOK_ADAPTER (object);

  if (!g_cancellable_is_cancelled (self->cancellable))
    g_cancellable_cancel (self->cancellable);

  if (self->registry != NULL)
    {
      g_signal_handlers_disconnect_by_data (self->registry, self);
      g_clear_object (&self->registry);
    }

  g_hash_table_remove_all (self->stores);

  G_OBJECT_CLASS (valent_ebook_adapter_parent_class)->dispose (object);
}

static void
valent_ebook_adapter_finalize (GObject *object)
{
  ValentEBookAdapter *self = VALENT_EBOOK_ADAPTER (object);

  g_clear_pointer (&self->stores, g_hash_table_unref);
  g_clear_object (&self->cancellable);

  G_OBJECT_CLASS (valent_ebook_adapter_parent_class)->finalize (object);
}

static void
valent_ebook_adapter_class_init (ValentEBookAdapterClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentContactsAdapterClass *adapter_class = VALENT_CONTACTS_ADAPTER_CLASS (klass);

  object_class->dispose = valent_ebook_adapter_dispose;
  object_class->finalize = valent_ebook_adapter_finalize;

  adapter_class->load_async = valent_ebook_adapter_load_async;
}

static void
valent_ebook_adapter_init (ValentEBookAdapter *self)
{
  self->cancellable = g_cancellable_new ();
  self->stores = g_hash_table_new_full ((GHashFunc)e_source_hash,
                                        (GEqualFunc)e_source_equal,
                                        g_object_unref,
                                        g_object_unref);
}

