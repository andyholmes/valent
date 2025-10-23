// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-notifications-adapter"

#include "config.h"

#include <libvalent-core.h>

#include "valent-notification.h"
#include "valent-notifications-adapter.h"


/**
 * ValentNotificationsAdapter:
 *
 * An abstract base class for notification servers.
 *
 * `ValentNotificationsAdapter` is a base class for notification servers. This
 * usually means monitoring a D-Bus service for notifications being sent and
 * withdrawn.
 *
 * ## `.plugin` File
 *
 * Implementations may define the following extra fields in the `.plugin` file:
 *
 * - `X-NotificationsAdapterPriority`
 *
 *     An integer indicating the adapter priority. The implementation with the
 *     lowest value will be used as the primary adapter.
 *
 * Since: 1.0
 */

typedef struct
{
  GPtrArray *items;
} ValentNotificationsAdapterPrivate;

static void   g_list_model_iface_init (GListModelInterface *iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (ValentNotificationsAdapter, valent_notifications_adapter, VALENT_TYPE_EXTENSION,
                                  G_ADD_PRIVATE (ValentNotificationsAdapter)
                                  G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, g_list_model_iface_init))

/*
 * GListModel
 */
static gpointer
valent_notifications_adapter_get_item (GListModel   *list,
                                       unsigned int  position)
{
  ValentNotificationsAdapter *self = VALENT_NOTIFICATIONS_ADAPTER (list);
  ValentNotificationsAdapterPrivate *priv = valent_notifications_adapter_get_instance_private (self);

  g_assert (VALENT_IS_NOTIFICATIONS_ADAPTER (self));

  if G_UNLIKELY (position >= priv->items->len)
    return NULL;

  return g_object_ref (g_ptr_array_index (priv->items, position));
}

static GType
valent_notifications_adapter_get_item_type (GListModel *list)
{
  return VALENT_TYPE_NOTIFICATION;
}

static unsigned int
valent_notifications_adapter_get_n_items (GListModel *list)
{
  ValentNotificationsAdapter *self = VALENT_NOTIFICATIONS_ADAPTER (list);
  ValentNotificationsAdapterPrivate *priv = valent_notifications_adapter_get_instance_private (self);

  g_assert (VALENT_IS_NOTIFICATIONS_ADAPTER (self));

  return priv->items->len;
}

static void
g_list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item = valent_notifications_adapter_get_item;
  iface->get_item_type = valent_notifications_adapter_get_item_type;
  iface->get_n_items = valent_notifications_adapter_get_n_items;
}

/*
 * GObject
 */
static void
valent_notifications_adapter_finalize (GObject *object)
{
  ValentNotificationsAdapter *self = VALENT_NOTIFICATIONS_ADAPTER (object);
  ValentNotificationsAdapterPrivate *priv = valent_notifications_adapter_get_instance_private (self);

  g_clear_pointer (&priv->items, g_ptr_array_unref);

  G_OBJECT_CLASS (valent_notifications_adapter_parent_class)->finalize (object);
}

static void
valent_notifications_adapter_class_init (ValentNotificationsAdapterClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = valent_notifications_adapter_finalize;
}

static void
valent_notifications_adapter_init (ValentNotificationsAdapter *adapter)
{
  ValentNotificationsAdapterPrivate *priv = valent_notifications_adapter_get_instance_private (adapter);

  priv->items = g_ptr_array_new_with_free_func (g_object_unref);
}

/**
 * valent_notifications_adapter_notification_added:
 * @adapter: a `ValentNotificationsAdapter`
 * @notification: a `ValentNotification`
 *
 * Called when @notification has been added to @adapter.
 *
 * This method should only be called by implementations of
 * [class@Valent.NotificationsAdapter]. @adapter will hold a reference on
 * @notification and emit [signal@Gio.ListModel::items-changed].
 *
 * Since: 1.0
 */
void
valent_notifications_adapter_notification_added (ValentNotificationsAdapter *adapter,
                                                 ValentNotification         *notification)
{
  ValentNotificationsAdapterPrivate *priv = valent_notifications_adapter_get_instance_private (adapter);
  unsigned int position = 0;

  g_return_if_fail (VALENT_IS_NOTIFICATIONS_ADAPTER (adapter));
  g_return_if_fail (VALENT_IS_NOTIFICATION (notification));

  position = priv->items->len;
  g_ptr_array_add (priv->items, g_object_ref (notification));
  g_list_model_items_changed (G_LIST_MODEL (adapter), position, 0, 1);
}

/**
 * valent_notifications_adapter_notification_removed:
 * @adapter: a `ValentNotificationsAdapter`
 * @notification: a `ValentNotification`
 *
 * Called when @notification has been removed from @adapter.
 *
 * This method should only be called by implementations of
 * [class@Valent.NotificationsAdapter]. @adapter will drop its reference on
 * @notification and emit [signal@Gio.ListModel::items-changed].
 *
 * Since: 1.0
 */
void
valent_notifications_adapter_notification_removed (ValentNotificationsAdapter *adapter,
                                                   ValentNotification         *notification)
{
  ValentNotificationsAdapterPrivate *priv = valent_notifications_adapter_get_instance_private (adapter);
  g_autoptr (ValentNotification) item = NULL;
  unsigned int position = 0;

  g_return_if_fail (VALENT_IS_NOTIFICATIONS_ADAPTER (adapter));
  g_return_if_fail (VALENT_IS_NOTIFICATION (notification));

  if (!g_ptr_array_find (priv->items, notification, &position))
    return;

  item = g_ptr_array_steal_index (priv->items, position);
  g_list_model_items_changed (G_LIST_MODEL (adapter), position, 1, 0);

  // TODO: avoid relying on the destroy signal with a state property
  valent_object_destroy (VALENT_OBJECT (notification));
}

