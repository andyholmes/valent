// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-notification-source"

#include "config.h"

#include <libvalent-core.h>

#include "valent-notification.h"
#include "valent-notification-source.h"


/**
 * SECTION:valent-notification-source
 * @short_description: Interface for notification sources
 * @title: ValentNotificationSource
 * @stability: Unstable
 * @include: libvalent-notifications.h
 *
 * #ValentNotificationSource is a base class for notification sources, which typically monitor the
 * DBus interface of notification servers. Implementations emit
 * #ValentNotificationSource::notification-added and #ValentNotificationSource::notification-removed
 * to indicate changes.
 *
 * It is possible to implement valent_notification_source_add_notification() and
 * valent_notification_source_remove_notification() to expose the ability to send notifications and
 * remove notifications on a specific service, but the methods provided by #ValentDevice and
 * #GApplication are more useful if you just want to send notifications to the host device.
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
   * The #PeasPluginInfo describing this source.
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
   * ValentNotificationSource::notification-added:
   * @source: a #ValentNotificationSource
   * @notification: a #GVariant
   *
   * ValentNotificationSource::notification-removed is emitted when a
   * notification is added to @source.
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
   * @id: a notification id
   *
   * ValentNotificationSource::notification-removed is emitted when a
   * notification is removed from @source.
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
 * @notification: a #GVariant
 *
 * Emits the #ValentNotificationSource::notification-added signal on @source.
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
 * Emits the #ValentNotificationSource::notification-removed signal on @source.
 */
void
valent_notification_source_emit_notification_removed (ValentNotificationSource *source,
                                                      const char               *id)
{
  g_return_if_fail (VALENT_IS_NOTIFICATION_SOURCE (source));

  g_signal_emit (G_OBJECT (source), signals [NOTIFICATION_REMOVED], 0, id);
}

/**
 * valent_notification_source_add_notification:
 * @source: a #ValentNotificationSource
 * @notification: a #ValentNotification
 *
 * Send @notification to the @source notification server.
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
 * valent_notification_source_remove_notification:
 * @source: a #ValentNotificationSource
 * @id: a notification id
 *
 * Withdraw @id from the @source notification server.
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
 * valent_notification_source_load_async:
 * @source: an #ValentNotificationSource
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): a #GAsyncReadyCallback
 * @user_data: (closure): user supplied data
 *
 * Requests that the #ValentNotificationSource asynchronously load any known players.
 *
 * This should only be called once on an #ValentNotificationSource. It is an error
 * to call this function more than once for a single #ValentNotificationSource.
 *
 * #ValentNotificationSource implementations are expected to emit the
 * #ValentNotificationSource::notification-added signal for each notification they've discovered.
 * That should be done for known players before returning from the asynchronous
 * operation so that the notification manager does not need to wait for additional
 * players to enter the "settled" state.
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
 * valent_notification_source_load_finish:
 * @source: an #ValentNotificationSource
 * @result: a #GAsyncResult provided to callback
 * @error: (nullable): a #GError
 *
 * Completes an asynchronous request to load known players via
 * valent_notification_source_load_async().
 *
 * Returns: %TRUE if successful; otherwise %FALSE and @error is set.
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

