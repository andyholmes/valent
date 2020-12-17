// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-contact-store-provider"

#include "config.h"

#include <gio/gio.h>
#include <libpeas/peas.h>
#include <libvalent-core.h>

#include "valent-contact-store.h"
#include "valent-contact-store-provider.h"


/**
 * SECTION:valent-contact-store-provider
 * @short_description: Interface for contact providers
 * @title: ValentContactStoreProvider
 * @stability: Unstable
 * @include: libvalent-contacts.h
 *
 * #ValentContactStoreProvider is an interface for sources that provide objects
 * that implement #ValentContactStore. The interface is very simple and only
 * requires that returned objects are implementations of #ValentContactStore.
 */

typedef struct
{
  PeasPluginInfo *plugin_info;

  GPtrArray      *stores;
} ValentContactStoreProviderPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (ValentContactStoreProvider, valent_contact_store_provider, G_TYPE_OBJECT)

/**
 * ValentContactStoreProviderClass:
 * @load_async: the virtual function pointer for valent_contact_store_provider_load_async()
 * @load_finish: the virtual function pointer for valent_contact_store_provider_load_finish()
 * @store_added: class closure for #ValentContactStoreProvider::store-added
 * @store_removed: class closure for #ValentContactStoreProvider::store-removed
 *
 * The virtual function table for #ValentContactStoreProvider.
 */

enum {
  PROP_0,
  PROP_PLUGIN_INFO,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

enum {
  STORE_ADDED,
  STORE_REMOVED,
  N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0, };


/* LCOV_EXCL_START */
static void
valent_contact_store_provider_real_load_async (ValentContactStoreProvider *provider,
                                               GCancellable               *cancellable,
                                               GAsyncReadyCallback         callback,
                                               gpointer                    user_data)
{
  g_task_report_new_error (provider, callback, user_data,
                           valent_contact_store_provider_real_load_async,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "%s does not implement load_async",
                           G_OBJECT_TYPE_NAME (provider));
}

static gboolean
valent_contact_store_provider_real_load_finish (ValentContactStoreProvider  *provider,
                                                GAsyncResult                *result,
                                                GError                     **error)
{
  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
valent_contact_store_provider_real_store_added (ValentContactStoreProvider *provider,
                                                ValentContactStore         *store)
{
  ValentContactStoreProviderPrivate *priv = valent_contact_store_provider_get_instance_private (provider);

  g_assert (VALENT_IS_CONTACT_STORE_PROVIDER (provider));
  g_assert (VALENT_IS_CONTACT_STORE (store));

  if (priv->stores == NULL)
    priv->stores = g_ptr_array_new_with_free_func (g_object_unref);
  g_ptr_array_add (priv->stores, g_object_ref (store));
}

static void
valent_contact_store_provider_real_store_removed (ValentContactStoreProvider *provider,
                                                  ValentContactStore         *store)
{
  ValentContactStoreProviderPrivate *priv = valent_contact_store_provider_get_instance_private (provider);

  g_assert (VALENT_IS_CONTACT_STORE_PROVIDER (provider));
  g_assert (VALENT_IS_CONTACT_STORE (store));

  /* Maybe we just disposed */
  if (priv->stores == NULL)
    return;

  if (!g_ptr_array_remove (priv->stores, store))
    g_warning ("No such store \"%s\" found in \"%s\"",
               G_OBJECT_TYPE_NAME (store),
               G_OBJECT_TYPE_NAME (provider));
}
/* LCOV_EXCL_STOP */


/*
 * GObject
 */
static void
valent_contact_store_provider_dispose (GObject *object)
{
  ValentContactStoreProvider *self = VALENT_CONTACT_STORE_PROVIDER (object);
  ValentContactStoreProviderPrivate *priv = valent_contact_store_provider_get_instance_private (self);

  g_clear_pointer (&priv->stores, g_ptr_array_unref);

  G_OBJECT_CLASS (valent_contact_store_provider_parent_class)->dispose (object);
}

static void
valent_contact_store_provider_get_property (GObject    *object,
                                            guint       prop_id,
                                            GValue     *value,
                                            GParamSpec *pspec)
{
  ValentContactStoreProvider *self = VALENT_CONTACT_STORE_PROVIDER (object);
  ValentContactStoreProviderPrivate *priv = valent_contact_store_provider_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_PLUGIN_INFO:
      g_value_set_boxed (value, priv->plugin_info);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_contact_store_provider_set_property (GObject      *object,
                                            guint         prop_id,
                                            const GValue *value,
                                            GParamSpec   *pspec)
{
  ValentContactStoreProvider *self = VALENT_CONTACT_STORE_PROVIDER (object);
  ValentContactStoreProviderPrivate *priv = valent_contact_store_provider_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_PLUGIN_INFO:
      priv->plugin_info = g_value_get_boxed (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_contact_store_provider_class_init (ValentContactStoreProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = valent_contact_store_provider_dispose;
  object_class->get_property = valent_contact_store_provider_get_property;
  object_class->set_property = valent_contact_store_provider_set_property;

  klass->load_async = valent_contact_store_provider_real_load_async;
  klass->load_finish = valent_contact_store_provider_real_load_finish;
  klass->store_added = valent_contact_store_provider_real_store_added;
  klass->store_removed = valent_contact_store_provider_real_store_removed;

  /**
   * ValentContactStoreProvider:plugin-info:
   *
   * The #PeasPluginInfo describing this provider.
   */
  properties [PROP_PLUGIN_INFO] =
    g_param_spec_boxed ("plugin-info",
                        "Plugin Info",
                        "Plugin Info",
                        PEAS_TYPE_PLUGIN_INFO,
                        (G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);

  /**
   * ValentContactStoreProvider::store-added:
   * @provider: an #ValentContactStoreProvider
   * @store: an #ValentContact
   *
   * The "store-added" signal is emitted when a provider has discovered a
   * store has become available.
   *
   * Subclasses of #ValentContactManager must chain-up if they override the
   * #ValentContactStoreProviderClass.store_added vfunc.
   */
  signals [STORE_ADDED] =
    g_signal_new ("store-added",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ValentContactStoreProviderClass, store_added),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, VALENT_TYPE_CONTACT_STORE);
  g_signal_set_va_marshaller (signals [STORE_ADDED],
                              G_TYPE_FROM_CLASS (klass),
                              g_cclosure_marshal_VOID__OBJECTv);

  /**
   * ValentContactStoreProvider::store-removed:
   * @provider: an #ValentContactStoreProvider
   * @store: an #ValentContact
   *
   * The "store-removed" signal is emitted when a provider has discovered a
   * store is no longer available.
   *
   * Subclasses of #ValentContactManager must chain-up if they override the
   * #ValentContactStoreProviderClass.store_removed vfunc.
   */
  signals [STORE_REMOVED] =
    g_signal_new ("store-removed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ValentContactStoreProviderClass, store_removed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, VALENT_TYPE_CONTACT_STORE);
  g_signal_set_va_marshaller (signals [STORE_REMOVED],
                              G_TYPE_FROM_CLASS (klass),
                              g_cclosure_marshal_VOID__OBJECTv);
}

static void
valent_contact_store_provider_init (ValentContactStoreProvider *provider)
{
  ValentContactStoreProviderPrivate *priv = valent_contact_store_provider_get_instance_private (provider);

  priv->stores = g_ptr_array_new_with_free_func (g_object_unref);
}

/**
 * valent_contact_store_provider_emit_store_added:
 * @provider: a #ValentContactStoreProvider
 * @store: a #ValentContactStore
 *
 * Emits the #ValentContactStoreProvider::store-added signal on @list.
 *
 * This function should only be called by classes implementing
 * #ValentContactStoreProvider. It has to be called after the internal representation
 * of @store has been updated, because handlers connected to this signal
 * might query the new state of the provider.
 */
void
valent_contact_store_provider_emit_store_added (ValentContactStoreProvider *provider,
                                                ValentContactStore         *store)
{
  g_return_if_fail (VALENT_IS_CONTACT_STORE_PROVIDER (provider));
  g_return_if_fail (VALENT_IS_CONTACT_STORE (store));

  g_signal_emit (G_OBJECT (provider), signals [STORE_ADDED], 0, store);
}

/**
 * valent_contact_store_provider_emit_store_removed:
 * @provider: a #ValentContactStoreProvider
 * @store: a #ValentContactStore
 *
 * Emits the #ValentContactStoreProvider::store-removed signal on @list.
 *
 * This function should only be called by classes implementing
 * #ValentContactStoreProvider.
 */
void
valent_contact_store_provider_emit_store_removed (ValentContactStoreProvider *provider,
                                                  ValentContactStore         *store)
{
  g_return_if_fail (VALENT_IS_CONTACT_STORE_PROVIDER (provider));
  g_return_if_fail (VALENT_IS_CONTACT_STORE (store));

  g_signal_emit (G_OBJECT (provider), signals [STORE_REMOVED], 0, store);
}

/**
 * valent_contact_store_provider_get_stores:
 * @provider: a #ValentContactStoreProvider
 *
 * Get a list of the contact stores known to @provider.
 *
 * Returns: (transfer container) (element-type Valent.ContactStore):
 *   a #GPtrArray of #ValentContactStore
 */
GPtrArray *
valent_contact_store_provider_get_stores (ValentContactStoreProvider *provider)
{
  ValentContactStoreProviderPrivate *priv = valent_contact_store_provider_get_instance_private (provider);
  g_autoptr (GPtrArray) stores = NULL;

  g_return_val_if_fail (VALENT_IS_CONTACT_STORE_PROVIDER (provider), NULL);

  stores = g_ptr_array_new_with_free_func (g_object_unref);

  for (unsigned int i = 0; i < priv->stores->len; i++)
    g_ptr_array_add (stores, g_object_ref (g_ptr_array_index (priv->stores, i)));

  return g_steal_pointer (&stores);
}

/**
 * valent_contact_store_provider_load_async:
 * @provider: an #ValentContactStoreProvider
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): a #GAsyncReadyCallback
 * @user_data: (closure): user supplied data
 *
 * Requests that the #ValentContactStoreProvider asynchronously load any known players.
 *
 * This should only be called once on an #ValentContactStoreProvider. It is an error
 * to call this function more than once for a single #ValentContactStoreProvider.
 *
 * #ValentContactStoreProvider implementations are expected to emit the
 * #ValentContactStoreProvider::store-added signal for each contact they've discovered.
 * That should be done for known players before returning from the asynchronous
 * operation so that the contact manager does not need to wait for additional
 * players to enter the "settled" state.
 */
void
valent_contact_store_provider_load_async (ValentContactStoreProvider *provider,
                                          GCancellable               *cancellable,
                                          GAsyncReadyCallback         callback,
                                          gpointer                    user_data)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_CONTACT_STORE_PROVIDER (provider));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  VALENT_CONTACT_STORE_PROVIDER_GET_CLASS (provider)->load_async (provider,
                                                                  cancellable,
                                                                  callback,
                                                                  user_data);

  VALENT_EXIT;
}

/**
 * valent_contact_store_provider_load_finish:
 * @provider: an #ValentContactStoreProvider
 * @result: a #GAsyncResult provided to callback
 * @error: (nullable): a #GError
 *
 * Completes an asynchronous request to load known players via
 * valent_contact_store_provider_load_async().
 *
 * Returns: %TRUE if successful; otherwise %FALSE and @error is set.
 */
gboolean
valent_contact_store_provider_load_finish (ValentContactStoreProvider  *provider,
                                           GAsyncResult                *result,
                                           GError                     **error)
{
  gboolean ret;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_CONTACT_STORE_PROVIDER (provider), FALSE);
  g_return_val_if_fail (g_task_is_valid (result, provider), FALSE);

  ret = VALENT_CONTACT_STORE_PROVIDER_GET_CLASS (provider)->load_finish (provider, result, error);

  VALENT_RETURN (ret);
}

