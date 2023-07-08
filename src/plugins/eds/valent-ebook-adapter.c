// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-ebook-adapter"

#include "config.h"

#include <gio/gio.h>
#include <valent.h>

#include "valent-ebook-adapter.h"
#include "valent-ebook-store.h"


struct _ValentEBookAdapter
{
  ValentContactsAdapter  parent_instance;

  ESourceRegistry       *registry;
  GHashTable            *stores;
};

static void   g_async_initable_iface_init (GAsyncInitableIface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (ValentEBookAdapter, valent_ebook_adapter, VALENT_TYPE_CONTACTS_ADAPTER,
                               G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE, g_async_initable_iface_init))


/*
 * ESourceRegistry Callbacks
 */
static void
g_async_initable_new_async_cb (GAsyncInitable     *initable,
                               GAsyncResult       *result,
                               ValentEBookAdapter *self)
{
  ESource *source = NULL;
  g_autoptr (GObject) object = NULL;
  g_autoptr (GError) error = NULL;

  if ((object = g_async_initable_new_finish (initable, result, &error)) == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Failed loading address book: %s", error->message);

      return;
    }

  source = valent_contact_store_get_source (VALENT_CONTACT_STORE (object));
  g_hash_table_replace (self->stores,
                        g_object_ref (source),
                        g_object_ref (object));
  valent_contacts_adapter_store_added (VALENT_CONTACTS_ADAPTER (self),
                                       VALENT_CONTACT_STORE (object));
}

static void
on_source_added (ESourceRegistry    *registry,
                 ESource            *source,
                 ValentEBookAdapter *self)
{
  g_autoptr (GCancellable) destroy = NULL;

  g_assert (E_IS_SOURCE_REGISTRY (registry));
  g_assert (E_IS_SOURCE (source));
  g_assert (VALENT_IS_EBOOK_ADAPTER (self));

  if (!e_source_has_extension (source, E_SOURCE_EXTENSION_ADDRESS_BOOK))
    return;

  destroy = valent_object_ref_cancellable (VALENT_OBJECT (self));
  g_async_initable_new_async (VALENT_TYPE_EBOOK_STORE,
                              G_PRIORITY_DEFAULT,
                              destroy,
                              (GAsyncReadyCallback)g_async_initable_new_async_cb,
                              self,
                              "source", source,
                              NULL);
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
      valent_contacts_adapter_store_removed (adapter, store);
      g_object_unref (esource);
      g_object_unref (store);
    }
}

/*
 * GAsyncInitable
 */
static void
e_source_registry_new_cb (GObject      *object,
                          GAsyncResult *result,
                          gpointer      user_data)
{
  g_autoptr (GTask) task = G_TASK (user_data);
  g_autolist (ESource) sources = NULL;
  ValentEBookAdapter *self = g_task_get_source_object (task);
  g_autoptr (GError) error = NULL;

  g_assert (VALENT_IS_EBOOK_ADAPTER (self));

  if ((self->registry = e_source_registry_new_finish (result, &error)) == NULL)
    {
      valent_extension_plugin_state_changed (VALENT_EXTENSION (self),
                                             VALENT_PLUGIN_STATE_ERROR,
                                             error);
      return g_task_return_error (task, g_steal_pointer (&error));
    }

  /* Load existing address books */
  sources = e_source_registry_list_sources (self->registry,
                                            E_SOURCE_EXTENSION_ADDRESS_BOOK);

  for (const GList *iter = sources; iter; iter = iter->next)
    on_source_added (self->registry, E_SOURCE (iter->data), self);

  g_signal_connect_object (self->registry,
                           "source-added",
                           G_CALLBACK (on_source_added),
                           self, 0);
  g_signal_connect_object (self->registry,
                           "source-removed",
                           G_CALLBACK (on_source_removed),
                           self, 0);

  /* Report the adapter as active */
  valent_extension_plugin_state_changed (VALENT_EXTENSION (self),
                                         VALENT_PLUGIN_STATE_ACTIVE,
                                         NULL);
  g_task_return_boolean (task, TRUE);
}

static void
valent_ebook_adapter_init_async (GAsyncInitable        *initable,
                                 int                    io_priority,
                                 GCancellable          *cancellable,
                                 GAsyncReadyCallback    callback,
                                 gpointer               user_data)
{
  g_autoptr (GTask) task = NULL;
  g_autoptr (GCancellable) destroy = NULL;

  g_assert (VALENT_IS_EBOOK_ADAPTER (initable));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  /* Cede the primary position until complete */
  valent_extension_plugin_state_changed (VALENT_EXTENSION (initable),
                                         VALENT_PLUGIN_STATE_INACTIVE,
                                         NULL);

  /* Cancel initialization if the object is destroyed */
  destroy = valent_object_chain_cancellable (VALENT_OBJECT (initable),
                                             cancellable);

  task = g_task_new (initable, destroy, callback, user_data);
  g_task_set_priority (task, io_priority);
  g_task_set_source_tag (task, valent_ebook_adapter_init_async);

  e_source_registry_new (destroy,
                         e_source_registry_new_cb,
                         g_steal_pointer (&task));
}

static void
g_async_initable_iface_init (GAsyncInitableIface *iface)
{
  iface->init_async = valent_ebook_adapter_init_async;
}

/*
 * ValentObject
 */
static void
valent_ebook_adapter_destroy (ValentObject *object)
{
  ValentEBookAdapter *self = VALENT_EBOOK_ADAPTER (object);

  g_clear_object (&self->registry);
  g_hash_table_remove_all (self->stores);

  VALENT_OBJECT_CLASS (valent_ebook_adapter_parent_class)->destroy (object);
}

/*
 * GObject
 */
static void
valent_ebook_adapter_finalize (GObject *object)
{
  ValentEBookAdapter *self = VALENT_EBOOK_ADAPTER (object);

  g_clear_pointer (&self->stores, g_hash_table_unref);

  G_OBJECT_CLASS (valent_ebook_adapter_parent_class)->finalize (object);
}

static void
valent_ebook_adapter_class_init (ValentEBookAdapterClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentObjectClass *vobject_class = VALENT_OBJECT_CLASS (klass);

  object_class->finalize = valent_ebook_adapter_finalize;

  vobject_class->destroy = valent_ebook_adapter_destroy;
}

static void
valent_ebook_adapter_init (ValentEBookAdapter *self)
{
  self->stores = g_hash_table_new_full ((GHashFunc)e_source_hash,
                                        (GEqualFunc)e_source_equal,
                                        g_object_unref,
                                        g_object_unref);
}

