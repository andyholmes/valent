// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

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

static void   g_list_model_iface_init (GListModelInterface *iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (ValentContactsAdapter, valent_contacts_adapter, VALENT_TYPE_OBJECT,
                                  G_ADD_PRIVATE (ValentContactsAdapter)
                                  G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, g_list_model_iface_init))

/**
 * ValentContactsAdapterClass:
 *
 * The virtual function table for #ValentContactsAdapter.
 */

enum {
  PROP_0,
  PROP_PLUGIN_INFO,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };


/*
 * GListModel
 */
static gpointer
valent_contacts_adapter_get_item (GListModel   *list,
                                  unsigned int  position)
{
  ValentContactsAdapter *self = VALENT_CONTACTS_ADAPTER (list);
  ValentContactsAdapterPrivate *priv = valent_contacts_adapter_get_instance_private (self);

  g_assert (VALENT_IS_CONTACTS_ADAPTER (self));

  if G_UNLIKELY (position >= priv->stores->len)
    return NULL;

  return g_object_ref (g_ptr_array_index (priv->stores, position));
}

static GType
valent_contacts_adapter_get_item_type (GListModel *list)
{
  return VALENT_TYPE_CONTACTS_ADAPTER;
}

static unsigned int
valent_contacts_adapter_get_n_items (GListModel *list)
{
  ValentContactsAdapter *self = VALENT_CONTACTS_ADAPTER (list);
  ValentContactsAdapterPrivate *priv = valent_contacts_adapter_get_instance_private (self);

  g_assert (VALENT_IS_CONTACTS_ADAPTER (self));

  return priv->stores->len;
}

static void
g_list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item = valent_contacts_adapter_get_item;
  iface->get_item_type = valent_contacts_adapter_get_item_type;
  iface->get_n_items = valent_contacts_adapter_get_n_items;
}

/*
 * GObject
 */
static void
valent_contacts_adapter_finalize (GObject *object)
{
  ValentContactsAdapter *self = VALENT_CONTACTS_ADAPTER (object);
  ValentContactsAdapterPrivate *priv = valent_contacts_adapter_get_instance_private (self);

  g_clear_pointer (&priv->stores, g_ptr_array_unref);

  G_OBJECT_CLASS (valent_contacts_adapter_parent_class)->finalize (object);
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

  object_class->finalize = valent_contacts_adapter_finalize;
  object_class->get_property = valent_contacts_adapter_get_property;
  object_class->set_property = valent_contacts_adapter_set_property;

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
 * Called when @store has been added to @adapter.
 *
 * This method should only be called by implementations of
 * [class@Valent.ContactsAdapter]. @adapter will hold a reference on @store and
 * emit [signal@Gio.ListModel::items-changed].
 *
 * Since: 1.0
 */
void
valent_contacts_adapter_store_added (ValentContactsAdapter *adapter,
                                     ValentContactStore    *store)
{
  ValentContactsAdapterPrivate *priv = valent_contacts_adapter_get_instance_private (adapter);
  unsigned int position = 0;

  g_return_if_fail (VALENT_IS_CONTACTS_ADAPTER (adapter));
  g_return_if_fail (VALENT_IS_CONTACT_STORE (store));

  position = priv->stores->len;
  g_ptr_array_add (priv->stores, g_object_ref (store));
  g_list_model_items_changed (G_LIST_MODEL (adapter), position, 0, 1);
}

/**
 * valent_contacts_adapter_store_removed:
 * @adapter: a #ValentContactsAdapter
 * @store: a #ValentContactStore
 *
 * Called when @store has been removed from @adapter.
 *
 * This method should only be called by implementations of
 * [class@Valent.ContactsAdapter]. @adapter will drop its reference on @store
 * and emit [signal@Gio.ListModel::items-changed].
 *
 * Since: 1.0
 */
void
valent_contacts_adapter_store_removed (ValentContactsAdapter *adapter,
                                       ValentContactStore    *store)
{
  ValentContactsAdapterPrivate *priv = valent_contacts_adapter_get_instance_private (adapter);
  g_autoptr (ValentContactStore) item = NULL;
  unsigned int position = 0;

  g_return_if_fail (VALENT_IS_CONTACTS_ADAPTER (adapter));
  g_return_if_fail (VALENT_IS_CONTACT_STORE (store));

  if (!g_ptr_array_find (priv->stores, store, &position))
    {
      g_warning ("No such store \"%s\" found in \"%s\"",
                 G_OBJECT_TYPE_NAME (store),
                 G_OBJECT_TYPE_NAME (adapter));
      return;
    }

  item = g_ptr_array_steal_index (priv->stores, position);
  g_list_model_items_changed (G_LIST_MODEL (adapter), position, 1, 0);
}

