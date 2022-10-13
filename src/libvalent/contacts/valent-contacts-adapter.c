// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-contacts-adapter"

#include "config.h"

#include <gio/gio.h>
#include <libpeas/peas.h>
#include <libvalent-core.h>

#include "valent-contact-store.h"
#include "valent-contacts-adapter.h"


/**
 * ValentContactsAdapter:
 *
 * An abstract base class for address book providers.
 *
 * #ValentContactsAdapter is a base class for plugins that provide an
 * interface to manage address books. This usually means monitoring and
 * querying [class@Valent.ContactStore] instances.
 *
 * ## `.plugin` File
 *
 * Implementations may define the following extra fields in the `.plugin` file:
 *
 * - `X-ContactsAdapterPriority`
 *
 *     An integer indicating the adapter priority. The implementation with the
 *     lowest value will be used as the primary adapter.
 *
 * Since: 1.0
 */

typedef struct
{
  PeasPluginInfo *plugin_info;
  GPtrArray      *stores;
} ValentContactsAdapterPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (ValentContactsAdapter, valent_contacts_adapter, G_TYPE_OBJECT)

/**
 * ValentContactsAdapterClass:
 * @store_added: class closure for #ValentContactsAdapter::store-added
 * @store_removed: class closure for #ValentContactsAdapter::store-removed
 *
 * The virtual function table for #ValentContactsAdapter.
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
valent_contacts_adapter_real_store_added (ValentContactsAdapter *adapter,
                                          ValentContactStore    *store)
{
  ValentContactsAdapterPrivate *priv = valent_contacts_adapter_get_instance_private (adapter);

  g_assert (VALENT_IS_CONTACTS_ADAPTER (adapter));
  g_assert (VALENT_IS_CONTACT_STORE (store));

  if (priv->stores == NULL)
    priv->stores = g_ptr_array_new_with_free_func (g_object_unref);
  g_ptr_array_add (priv->stores, g_object_ref (store));
}

static void
valent_contacts_adapter_real_store_removed (ValentContactsAdapter *adapter,
                                            ValentContactStore    *store)
{
  ValentContactsAdapterPrivate *priv = valent_contacts_adapter_get_instance_private (adapter);

  g_assert (VALENT_IS_CONTACTS_ADAPTER (adapter));
  g_assert (VALENT_IS_CONTACT_STORE (store));

  /* Maybe we just disposed */
  if (priv->stores == NULL)
    return;

  if (!g_ptr_array_remove (priv->stores, store))
    g_warning ("No such store \"%s\" found in \"%s\"",
               G_OBJECT_TYPE_NAME (store),
               G_OBJECT_TYPE_NAME (adapter));
}
/* LCOV_EXCL_STOP */

/*
 * GObject
 */
static void
valent_contacts_adapter_dispose (GObject *object)
{
  ValentContactsAdapter *self = VALENT_CONTACTS_ADAPTER (object);
  ValentContactsAdapterPrivate *priv = valent_contacts_adapter_get_instance_private (self);

  g_clear_pointer (&priv->stores, g_ptr_array_unref);

  G_OBJECT_CLASS (valent_contacts_adapter_parent_class)->dispose (object);
}

static void
valent_contacts_adapter_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  ValentContactsAdapter *self = VALENT_CONTACTS_ADAPTER (object);
  ValentContactsAdapterPrivate *priv = valent_contacts_adapter_get_instance_private (self);

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
valent_contacts_adapter_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  ValentContactsAdapter *self = VALENT_CONTACTS_ADAPTER (object);
  ValentContactsAdapterPrivate *priv = valent_contacts_adapter_get_instance_private (self);

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
valent_contacts_adapter_class_init (ValentContactsAdapterClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = valent_contacts_adapter_dispose;
  object_class->get_property = valent_contacts_adapter_get_property;
  object_class->set_property = valent_contacts_adapter_set_property;

  klass->store_added = valent_contacts_adapter_real_store_added;
  klass->store_removed = valent_contacts_adapter_real_store_removed;

  /**
   * ValentContactsAdapter:plugin-info:
   *
   * The [struct@Peas.PluginInfo] describing this adapter.
   *
   * Since: 1.0
   */
  properties [PROP_PLUGIN_INFO] =
    g_param_spec_boxed ("plugin-info", NULL, NULL,
                        PEAS_TYPE_PLUGIN_INFO,
                        (G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);

  /**
   * ValentContactsAdapter::store-added:
   * @adapter: an #ValentContactsAdapter
   * @store: an #ValentContact
   *
   * Emitted when a [class@Valent.ContactStore] has been added to @adapter.
   *
   * Implementations of #ValentContactsAdapter must chain-up if they
   * override [vfunc@Valent.ContactsAdapter.store_added].
   *
   * Since: 1.0
   */
  signals [STORE_ADDED] =
    g_signal_new ("store-added",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (ValentContactsAdapterClass, store_added),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, VALENT_TYPE_CONTACT_STORE);
  g_signal_set_va_marshaller (signals [STORE_ADDED],
                              G_TYPE_FROM_CLASS (klass),
                              g_cclosure_marshal_VOID__OBJECTv);

  /**
   * ValentContactsAdapter::store-removed:
   * @adapter: an #ValentContactsAdapter
   * @store: an #ValentContact
   *
   * Emitted when a [class@Valent.ContactStore] has been removed from @adapter.
   *
   * Implementations of #ValentContactsAdapter must chain-up if they
   * override [vfunc@Valent.ContactsAdapter.store_removed].
   *
   * Since: 1.0
   */
  signals [STORE_REMOVED] =
    g_signal_new ("store-removed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (ValentContactsAdapterClass, store_removed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, VALENT_TYPE_CONTACT_STORE);
  g_signal_set_va_marshaller (signals [STORE_REMOVED],
                              G_TYPE_FROM_CLASS (klass),
                              g_cclosure_marshal_VOID__OBJECTv);
}

static void
valent_contacts_adapter_init (ValentContactsAdapter *adapter)
{
  ValentContactsAdapterPrivate *priv = valent_contacts_adapter_get_instance_private (adapter);

  priv->stores = g_ptr_array_new_with_free_func (g_object_unref);
}

/**
 * valent_contacts_adapter_store_added:
 * @adapter: a #ValentContactsAdapter
 * @store: a #ValentContactStore
 *
 * Emit [signal@Valent.ContactsAdapter::store-added] on @adapter.
 *
 * This method should only be called by implementations of
 * [class@Valent.ContactsAdapter].
 *
 * Since: 1.0
 */
void
valent_contacts_adapter_store_added (ValentContactsAdapter *adapter,
                                     ValentContactStore    *store)
{
  g_return_if_fail (VALENT_IS_CONTACTS_ADAPTER (adapter));
  g_return_if_fail (VALENT_IS_CONTACT_STORE (store));

  g_signal_emit (G_OBJECT (adapter), signals [STORE_ADDED], 0, store);
}

/**
 * valent_contacts_adapter_store_removed:
 * @adapter: a #ValentContactsAdapter
 * @store: a #ValentContactStore
 *
 * Emit [signal@Valent.ContactsAdapter::store-removed] on @adapter.
 *
 * This method should only be called by implementations of
 * [class@Valent.ContactsAdapter].
 *
 * Since: 1.0
 */
void
valent_contacts_adapter_store_removed (ValentContactsAdapter *adapter,
                                       ValentContactStore    *store)
{
  g_return_if_fail (VALENT_IS_CONTACTS_ADAPTER (adapter));
  g_return_if_fail (VALENT_IS_CONTACT_STORE (store));

  g_object_ref (store);
  g_signal_emit (G_OBJECT (adapter), signals [STORE_REMOVED], 0, store);
  g_object_unref (store);
}

/**
 * valent_contacts_adapter_get_stores:
 * @adapter: a #ValentContactsAdapter
 *
 * Get a list of the contact stores known to @adapter.
 *
 * Returns: (transfer container) (element-type Valent.ContactStore): a list of
 *     stores
 *
 * Since: 1.0
 */
GPtrArray *
valent_contacts_adapter_get_stores (ValentContactsAdapter *adapter)
{
  ValentContactsAdapterPrivate *priv = valent_contacts_adapter_get_instance_private (adapter);
  GPtrArray *ret;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_CONTACTS_ADAPTER (adapter), NULL);

  ret = g_ptr_array_new_with_free_func (g_object_unref);

  for (unsigned int i = 0; i < priv->stores->len; i++)
    g_ptr_array_add (ret, g_object_ref (g_ptr_array_index (priv->stores, i)));

  VALENT_RETURN (ret);
}

