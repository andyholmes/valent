// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

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
 * #ValentNotificationsAdapter is a base class for notification servers. This
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
  PeasPluginInfo *plugin_info;
} ValentNotificationsAdapterPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (ValentNotificationsAdapter, valent_notifications_adapter, G_TYPE_OBJECT)

/**
 * ValentNotificationsAdapterClass:
 * @add_notification: the virtual function pointer for valent_notifications_adapter_add_notification()
 * @remove_notification: the virtual function pointer for valent_notifications_adapter_remove_notification()
 * @notification_added: the class closure for #ValentNotificationsAdapter::notification-added signal
 * @notification_removed: the class closure for #ValentNotificationsAdapter::notification-removed signal
 *
 * The virtual function table for #ValentNotificationsAdapter.
 */

enum {
  PROP_0,
  PROP_PLUGIN_INFO,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

enum {
  NOTIFICATION_ADDED,
  NOTIFICATION_REMOVED,
  N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0, };


/* LCOV_EXCL_START */
static void
valent_notifications_adapter_real_add_notification (ValentNotificationsAdapter *adapter,
                                                    ValentNotification         *notification)
{
}

static void
valent_notifications_adapter_real_remove_notification (ValentNotificationsAdapter *adapter,
                                                       const char                 *id)
{
}
/* LCOV_EXCL_STOP */

/*
 * GObject
 */
static void
valent_notifications_adapter_get_property (GObject    *object,
                                         guint       prop_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
  ValentNotificationsAdapter *self = VALENT_NOTIFICATIONS_ADAPTER (object);
  ValentNotificationsAdapterPrivate *priv = valent_notifications_adapter_get_instance_private (self);

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
valent_notifications_adapter_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  ValentNotificationsAdapter *self = VALENT_NOTIFICATIONS_ADAPTER (object);
  ValentNotificationsAdapterPrivate *priv = valent_notifications_adapter_get_instance_private (self);

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
valent_notifications_adapter_class_init (ValentNotificationsAdapterClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = valent_notifications_adapter_get_property;
  object_class->set_property = valent_notifications_adapter_set_property;

  klass->add_notification = valent_notifications_adapter_real_add_notification;
  klass->remove_notification = valent_notifications_adapter_real_remove_notification;

  /**
   * ValentNotificationsAdapter:plugin-info:
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
   * ValentNotificationsAdapter::notification-added:
   * @adapter: a #ValentNotificationsAdapter
   * @notification: a #ValentNotification
   *
   * Emitted when a [class@Valent.Notification] is added to @adapter.
   *
   * Implementations must chain up if they override
   * [vfunc@Valent.NotificationsAdapter.notification_added].
   *
   * Since: 1.0
   */
  signals [NOTIFICATION_ADDED] =
    g_signal_new ("notification-added",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ValentNotificationsAdapterClass, notification_added),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, VALENT_TYPE_NOTIFICATION);
  g_signal_set_va_marshaller (signals [NOTIFICATION_ADDED],
                              G_TYPE_FROM_CLASS (klass),
                              g_cclosure_marshal_VOID__OBJECTv);

  /**
   * ValentNotificationsAdapter::notification-removed:
   * @adapter: a #ValentNotificationsAdapter
   * @notification: a #ValentNotification
   *
   * Emitted when a [class@Valent.Notification] is removed from @adapter.
   *
   * Implementations must chain up if they override
   * [vfunc@Valent.NotificationsAdapter.notification_removed].
   *
   * Since: 1.0
   */
  signals [NOTIFICATION_REMOVED] =
    g_signal_new ("notification-removed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ValentNotificationsAdapterClass, notification_removed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__STRING,
                  G_TYPE_NONE, 1, G_TYPE_STRING);
  g_signal_set_va_marshaller (signals [NOTIFICATION_REMOVED],
                              G_TYPE_FROM_CLASS (klass),
                              g_cclosure_marshal_VOID__STRINGv);
}

static void
valent_notifications_adapter_init (ValentNotificationsAdapter *adapter)
{
}

/**
 * valent_notifications_adapter_notification_added:
 * @adapter: a #ValentNotificationsAdapter
 * @notification: a #ValentNotification
 *
 * Emit [signal@Valent.NotificationsAdapter::notification-added] on @adapter.
 *
 * This method should only be called by implementations of
 * [class@Valent.NotificationsAdapter].
 *
 * Since: 1.0
 */
void
valent_notifications_adapter_notification_added (ValentNotificationsAdapter *adapter,
                                                 ValentNotification         *notification)
{
  g_return_if_fail (VALENT_IS_NOTIFICATIONS_ADAPTER (adapter));

  g_signal_emit (G_OBJECT (adapter), signals [NOTIFICATION_ADDED], 0, notification);
}

/**
 * valent_notifications_adapter_notification_removed:
 * @adapter: a #ValentNotificationsAdapter
 * @id: a notification id
 *
 * Emit [signal@Valent.NotificationsAdapter::notification-removed] on @adapter.
 *
 * This method should only be called by implementations of
 * [class@Valent.NotificationsAdapter].
 *
 * Since: 1.0
 */
void
valent_notifications_adapter_notification_removed (ValentNotificationsAdapter *adapter,
                                                   const char                 *id)
{
  g_return_if_fail (VALENT_IS_NOTIFICATIONS_ADAPTER (adapter));

  g_signal_emit (G_OBJECT (adapter), signals [NOTIFICATION_REMOVED], 0, id);
}

/**
 * valent_notifications_adapter_add_notification: (virtual add_notification)
 * @adapter: a #ValentNotificationsAdapter
 * @notification: a #ValentNotification
 *
 * Send @notification to the @adapter.
 *
 * Since: 1.0
 */
void
valent_notifications_adapter_add_notification (ValentNotificationsAdapter *adapter,
                                               ValentNotification         *notification)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_NOTIFICATIONS_ADAPTER (adapter));
  g_return_if_fail (VALENT_IS_NOTIFICATION (notification));

  VALENT_NOTIFICATIONS_ADAPTER_GET_CLASS (adapter)->add_notification (adapter,
                                                                      notification);

  VALENT_EXIT;
}

/**
 * valent_notifications_adapter_remove_notification: (virtual remove_notification)
 * @adapter: a #ValentNotificationsAdapter
 * @id: a notification id
 *
 * Withdraw @id from @adapter.
 *
 * Since: 1.0
 */
void
valent_notifications_adapter_remove_notification (ValentNotificationsAdapter *adapter,
                                                  const char                 *id)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_NOTIFICATIONS_ADAPTER (adapter));
  g_return_if_fail (id == NULL);

  VALENT_NOTIFICATIONS_ADAPTER_GET_CLASS (adapter)->remove_notification (adapter,
                                                                         id);

  VALENT_EXIT;
}

