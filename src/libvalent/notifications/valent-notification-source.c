// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-notification-source"

#include "config.h"

#include <libvalent-core.h>

#include "valent-notification.h"
#include "valent-notification-source.h"


/**
 * ValentNotificationSource:
 *
 * An abstract base class for notification servers.
 *
 * #ValentNotificationSource is a base class for notification servers. This
 * usually means monitoring a D-Bus service for notifications being sent and
 * withdrawn.
 *
 * ## `.plugin` File
 *
 * Implementations may define the following extra fields in the `.plugin` file:
 *
 * - `X-NotificationSourcePriority`
 *
 *     An integer indicating the adapter priority. The implementation with the
 *     lowest value will be used as the primary adapter.
 *
 * Since: 1.0
 */

typedef struct
{
  PeasPluginInfo *plugin_info;
} ValentNotificationSourcePrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (ValentNotificationSource, valent_notification_source, G_TYPE_OBJECT)

/**
 * ValentNotificationSourceClass:
 * @add_notification: the virtual function pointer for valent_notification_source_add_notification()
 * @remove_notification: the virtual function pointer for valent_notification_source_remove_notification()
 * @load_async: the virtual function pointer for valent_notification_source_load_async()
 * @load_finish: the virtual function pointer for valent_notification_source_load_finish()
 * @notification_added: the class closure for #ValentNotificationSource::notification-added signal
 * @notification_removed: the class closure for #ValentNotificationSource::notification-removed signal
 *
 * The virtual function table for #ValentNotificationSource.
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
valent_notification_source_real_add_notification (ValentNotificationSource *source,
                                                  ValentNotification       *notification)
{
}

static void
valent_notification_source_real_remove_notification (ValentNotificationSource *source,
                                                     const char               *id)
{
}

static void
valent_notification_source_real_load_async (ValentNotificationSource *source,
                                            GCancellable             *cancellable,
                                            GAsyncReadyCallback       callback,
                                            gpointer                  user_data)
{
  g_task_report_new_error (source, callback, user_data,
                           valent_notification_source_real_load_async,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "%s does not implement load_async",
                           G_OBJECT_TYPE_NAME (source));
}

static gboolean
valent_notification_source_real_load_finish (ValentNotificationSource  *source,
                                             GAsyncResult              *result,
                                             GError                   **error)
{
  g_assert (VALENT_IS_NOTIFICATION_SOURCE (source));
  g_assert (g_task_is_valid (result, source));
  g_assert (error == NULL || *error == NULL);

  return g_task_propagate_boolean (G_TASK (result), error);
}
/* LCOV_EXCL_STOP */

/*
 * GObject
 */
static void
valent_notification_source_get_property (GObject    *object,
                                         guint       prop_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
  ValentNotificationSource *self = VALENT_NOTIFICATION_SOURCE (object);
  ValentNotificationSourcePrivate *priv = valent_notification_source_get_instance_private (self);

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
valent_notification_source_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  ValentNotificationSource *self = VALENT_NOTIFICATION_SOURCE (object);
  ValentNotificationSourcePrivate *priv = valent_notification_source_get_instance_private (self);

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
valent_notification_source_class_init (ValentNotificationSourceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = valent_notification_source_get_property;
  object_class->set_property = valent_notification_source_set_property;

  klass->add_notification = valent_notification_source_real_add_notification;
  klass->remove_notification = valent_notification_source_real_remove_notification;
  klass->load_async = valent_notification_source_real_load_async;
  klass->load_finish = valent_notification_source_real_load_finish;

  /**
   * ValentNotificationSource:plugin-info:
   *
   * The [struct@Peas.PluginInfo] describing this adapter.
   *
   * Since: 1.0
   */
  properties [PROP_PLUGIN_INFO] =
    g_param_spec_boxed ("plugin-info",
                        "Plugin Info",
                        "The plugin info describing this adapter",
                        PEAS_TYPE_PLUGIN_INFO,
                        (G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);

  /**
   * ValentNotificationSource::notification-added:
   * @source: a #ValentNotificationSource
   * @notification: a #ValentNotification
   *
   * Emitted when a [class@Valent.Notification] is added to @source.
   *
   * Implementations must chain up if they override
   * [vfunc@Valent.NotificationSource.notification_added].
   *
   * Since: 1.0
   */
  signals [NOTIFICATION_ADDED] =
    g_signal_new ("notification-added",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ValentNotificationSourceClass, notification_added),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, VALENT_TYPE_NOTIFICATION);
  g_signal_set_va_marshaller (signals [NOTIFICATION_ADDED],
                              G_TYPE_FROM_CLASS (klass),
                              g_cclosure_marshal_VOID__OBJECTv);

  /**
   * ValentNotificationSource::notification-removed:
   * @source: a #ValentNotificationSource
   * @notification: a #ValentNotification
   *
   * Emitted when a [class@Valent.Notification] is removed from @source.
   *
   * Implementations must chain up if they override
   * [vfunc@Valent.NotificationSource.notification_removed].
   *
   * Since: 1.0
   */
  signals [NOTIFICATION_REMOVED] =
    g_signal_new ("notification-removed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ValentNotificationSourceClass, notification_removed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__STRING,
                  G_TYPE_NONE, 1, G_TYPE_STRING);
  g_signal_set_va_marshaller (signals [NOTIFICATION_REMOVED],
                              G_TYPE_FROM_CLASS (klass),
                              g_cclosure_marshal_VOID__STRINGv);
}

static void
valent_notification_source_init (ValentNotificationSource *source)
{
}

/**
 * valent_notification_source_emit_notification_added:
 * @source: a #ValentNotificationSource
 * @notification: a #ValentNotification
 *
 * Emit [signal@Valent.NotificationSource::notification-added] on @source.
 *
 * This method should only be called by implementations of
 * [class@Valent.NotificationSource].
 *
 * Since: 1.0
 */
void
valent_notification_source_emit_notification_added (ValentNotificationSource *source,
                                                    ValentNotification       *notification)
{
  g_return_if_fail (VALENT_IS_NOTIFICATION_SOURCE (source));

  g_signal_emit (G_OBJECT (source), signals [NOTIFICATION_ADDED], 0, notification);
}

/**
 * valent_notification_source_emit_notification_removed:
 * @source: a #ValentNotificationSource
 * @id: a notification id
 *
 * Emit [signal@Valent.NotificationSource::notification-removed] on @source.
 *
 * This method should only be called by implementations of
 * [class@Valent.NotificationSource].
 *
 * Since: 1.0
 */
void
valent_notification_source_emit_notification_removed (ValentNotificationSource *source,
                                                      const char               *id)
{
  g_return_if_fail (VALENT_IS_NOTIFICATION_SOURCE (source));

  g_signal_emit (G_OBJECT (source), signals [NOTIFICATION_REMOVED], 0, id);
}

/**
 * valent_notification_source_add_notification: (virtual add_notification)
 * @source: a #ValentNotificationSource
 * @notification: a #ValentNotification
 *
 * Send @notification to the @source.
 *
 * Since: 1.0
 */
void
valent_notification_source_add_notification (ValentNotificationSource *source,
                                             ValentNotification       *notification)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_NOTIFICATION_SOURCE (source));
  g_return_if_fail (VALENT_IS_NOTIFICATION (notification));

  VALENT_NOTIFICATION_SOURCE_GET_CLASS (source)->add_notification (source,
                                                                   notification);

  VALENT_EXIT;
}

/**
 * valent_notification_source_remove_notification: (virtual remove_notification)
 * @source: a #ValentNotificationSource
 * @id: a notification id
 *
 * Withdraw @id from @source.
 *
 * Since: 1.0
 */
void
valent_notification_source_remove_notification (ValentNotificationSource *source,
                                                const char               *id)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_NOTIFICATION_SOURCE (source));
  g_return_if_fail (id == NULL);

  VALENT_NOTIFICATION_SOURCE_GET_CLASS (source)->remove_notification (source,
                                                                      id);

  VALENT_EXIT;
}

/**
 * valent_notification_source_load_async: (virtual load_async)
 * @source: an #ValentNotificationSource
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): a #GAsyncReadyCallback
 * @user_data: (closure): user supplied data
 *
 * Load any notifications known to @source.
 *
 * Implementations are expected to emit
 * [signal@Valent.NotificationSource::notification-added] for each notification
 * before completing the operation.
 *
 * This method is called by the [class@Valent.Notifications] singleton and must
 * only be called once for each implementation. It is therefore a programmer
 * error for an API user to call this method.
 *
 * Since: 1.0
 */
void
valent_notification_source_load_async (ValentNotificationSource *source,
                                       GCancellable             *cancellable,
                                       GAsyncReadyCallback       callback,
                                       gpointer                  user_data)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_NOTIFICATION_SOURCE (source));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  VALENT_NOTIFICATION_SOURCE_GET_CLASS (source)->load_async (source,
                                                             cancellable,
                                                             callback,
                                                             user_data);

  VALENT_EXIT;
}

/**
 * valent_notification_source_load_finish: (virtual load_finish)
 * @source: an #ValentNotificationSource
 * @result: a #GAsyncResult provided to callback
 * @error: (nullable): a #GError
 *
 * Finish an operation started by [method@Valent.NotificationSource.load_async].
 *
 * Returns: %TRUE if successful, or %FALSE with @error set
 *
 * Since: 1.0
 */
gboolean
valent_notification_source_load_finish (ValentNotificationSource  *source,
                                        GAsyncResult              *result,
                                        GError                   **error)
{
  gboolean ret;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_NOTIFICATION_SOURCE (source), FALSE);
  g_return_val_if_fail (g_task_is_valid (result, source), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  ret = VALENT_NOTIFICATION_SOURCE_GET_CLASS (source)->load_finish (source,
                                                                    result,
                                                                    error);

  VALENT_RETURN (ret);
}

