// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-notifications"

#include "config.h"

#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>
#include <libpeas.h>
#include <libvalent-core.h>

#include "valent-notification.h"
#include "valent-notifications.h"
#include "valent-notifications-adapter.h"


/**
 * ValentNotifications:
 *
 * A class for sending and receiving notifications.
 *
 * #ValentNotifications is an aggregator of notifications, intended for use by
 * [class@Valent.DevicePlugin] implementations.
 *
 * Plugins can implement [class@Valent.NotificationsAdapter] to provide an
 * interface to monitor, send and withdraw notifications.
 *
 * Since: 1.0
 */

struct _ValentNotifications
{
  ValentComponent  parent_instance;

  GVariant        *applications;
};

G_DEFINE_FINAL_TYPE (ValentNotifications, valent_notifications, VALENT_TYPE_COMPONENT)

enum {
  NOTIFICATION_ADDED,
  NOTIFICATION_REMOVED,
  N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0, };


static ValentNotifications *default_listener = NULL;


static GVariant *
app_info_serialize (GAppInfo *info)
{
  GVariantDict dict;
  const char *name;
  GIcon *icon;

  g_assert (G_IS_APP_INFO (info));

  g_variant_dict_init (&dict, NULL);

  if ((name = g_app_info_get_display_name (info)) != NULL)
    g_variant_dict_insert (&dict, "name", "s", name);

  if ((icon = g_app_info_get_icon (info)) != NULL)
    {
      g_autoptr (GVariant) iconv = NULL;

      iconv = g_icon_serialize (icon);
      g_variant_dict_insert_value (&dict, "icon", iconv);
    }

  return g_variant_dict_end (&dict);
}

static GVariant *
notification_serialize (ValentNotification *notification)
{
  GVariantDict dict;
  const char *app_name;
  GIcon *icon;

  g_variant_dict_init (&dict, NULL);

  if ((app_name = valent_notification_get_application (notification)) != NULL)
    g_variant_dict_insert (&dict, "name", "s", app_name);

  if ((icon = valent_notification_get_icon (notification)) != NULL)
    {
      g_autoptr (GVariant) iconv = NULL;

      iconv = g_icon_serialize (icon);
      g_variant_dict_insert_value (&dict, "icon", iconv);
    }

  return g_variant_dict_end (&dict);
}

static void
query_applications (ValentNotifications *self)
{
  GVariantDict dict;
  g_autolist (GAppInfo) infos = NULL;

  g_assert (VALENT_IS_NOTIFICATIONS (self));

  g_variant_dict_init (&dict, NULL);
  infos = g_app_info_get_all ();

  for (const GList *iter = infos; iter; iter = iter->next)
    {
      const char *desktop_id;
      const char *name;

      desktop_id = g_app_info_get_id (iter->data);

      if G_UNLIKELY (g_str_has_prefix (desktop_id, APPLICATION_ID))
        continue;

      if (!g_desktop_app_info_get_boolean (iter->data, "X-GNOME-UsesNotifications"))
        continue;

      name = g_app_info_get_display_name (iter->data);
      g_variant_dict_insert_value (&dict, name, app_info_serialize (iter->data));
    }

  self->applications = g_variant_ref_sink (g_variant_dict_end (&dict));
}

static void
add_application (ValentNotifications *self,
                 GVariant            *application)
{
  GVariantDict dict;
  const char *name;

  if (self->applications == NULL)
    query_applications (self);

  g_variant_dict_init (&dict, self->applications);

  if (g_variant_lookup (application, "name", "&s", &name))
    g_variant_dict_insert_value (&dict, name, g_variant_ref_sink (application));
  g_variant_unref (application);

  g_clear_pointer (&self->applications, g_variant_unref);
  self->applications = g_variant_ref_sink (g_variant_dict_end (&dict));
}


/*
 * ValentNotificationsAdapter Callbacks
 */
static void
on_notification_added (ValentNotificationsAdapter *adapter,
                       ValentNotification         *notification,
                       ValentNotifications        *self)
{
  g_assert (VALENT_IS_NOTIFICATIONS_ADAPTER (adapter));
  g_assert (VALENT_IS_NOTIFICATION (notification));
  g_assert (VALENT_IS_NOTIFICATIONS (self));

  add_application (self, notification_serialize (notification));

  g_signal_emit (G_OBJECT (self), signals [NOTIFICATION_ADDED], 0, notification);
}

static void
on_notification_removed (ValentNotificationsAdapter *adapter,
                         const char                 *id,
                         ValentNotifications        *self)
{
  g_assert (VALENT_IS_NOTIFICATIONS_ADAPTER (adapter));
  g_assert (id != NULL);
  g_assert (VALENT_IS_NOTIFICATIONS (self));

  g_signal_emit (G_OBJECT (self), signals [NOTIFICATION_REMOVED], 0, id);
}


/*
 * ValentComponent
 */
static void
valent_notifications_bind_extension (ValentComponent *component,
                                     GObject         *extension)
{
  ValentNotifications *self = VALENT_NOTIFICATIONS (component);
  ValentNotificationsAdapter *adapter = VALENT_NOTIFICATIONS_ADAPTER (extension);

  VALENT_ENTRY;

  g_assert (VALENT_IS_NOTIFICATIONS (self));
  g_assert (VALENT_IS_NOTIFICATIONS_ADAPTER (adapter));

  g_signal_connect_object (adapter,
                           "notification-added",
                           G_CALLBACK (on_notification_added),
                           self, 0);

  g_signal_connect_object (adapter,
                           "notification-removed",
                           G_CALLBACK (on_notification_removed),
                           self, 0);

  VALENT_EXIT;
}

static void
valent_notifications_unbind_extension (ValentComponent *component,
                                       GObject         *extension)
{
  ValentNotifications *self = VALENT_NOTIFICATIONS (component);
  ValentNotificationsAdapter *adapter = VALENT_NOTIFICATIONS_ADAPTER (extension);

  g_assert (VALENT_IS_NOTIFICATIONS (self));
  g_assert (VALENT_IS_NOTIFICATIONS_ADAPTER (adapter));

  g_signal_handlers_disconnect_by_func (adapter, on_notification_added, self);
  g_signal_handlers_disconnect_by_func (adapter, on_notification_removed, self);
}


/*
 * GObject
 */
static void
valent_notifications_finalize (GObject *object)
{
  ValentNotifications *self = VALENT_NOTIFICATIONS (object);

  g_clear_pointer (&self->applications, g_variant_unref);

  G_OBJECT_CLASS (valent_notifications_parent_class)->finalize (object);
}

static void
valent_notifications_class_init (ValentNotificationsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentComponentClass *component_class = VALENT_COMPONENT_CLASS (klass);

  object_class->finalize = valent_notifications_finalize;

  component_class->bind_extension = valent_notifications_bind_extension;
  component_class->unbind_extension = valent_notifications_unbind_extension;

  /**
   * ValentNotifications::notification-added:
   * @notifications: a #ValentNotifications
   * @notification: a #ValentNotification
   *
   * Emitted when a notification is added to a
   * [class@Valent.NotificationsAdapter].
   *
   * Since: 1.0
   */
  signals [NOTIFICATION_ADDED] =
    g_signal_new ("notification-added",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, VALENT_TYPE_NOTIFICATION);
  g_signal_set_va_marshaller (signals [NOTIFICATION_ADDED],
                              G_TYPE_FROM_CLASS (klass),
                              g_cclosure_marshal_VOID__OBJECTv);

  /**
   * ValentNotifications::notification-removed:
   * @notifications: a #ValentNotifications
   * @id: a notification id
   *
   * Emitted when a notification is removed from a
   * [class@Valent.NotificationsAdapter].
   *
   * Since: 1.0
   */
  signals [NOTIFICATION_REMOVED] =
    g_signal_new ("notification-removed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__STRING,
                  G_TYPE_NONE, 1, G_TYPE_STRING);
  g_signal_set_va_marshaller (signals [NOTIFICATION_REMOVED],
                              G_TYPE_FROM_CLASS (klass),
                              g_cclosure_marshal_VOID__STRINGv);
}

static void
valent_notifications_init (ValentNotifications *self)
{
  query_applications (self);
}

/**
 * valent_notifications_get_default:
 *
 * Get the default [class@Valent.Notifications].
 *
 * Returns: (transfer none) (not nullable): a #ValentNotifications
 *
 * Since: 1.0
 */
ValentNotifications *
valent_notifications_get_default (void)
{
  if (default_listener == NULL)
    {
      default_listener = g_object_new (VALENT_TYPE_NOTIFICATIONS,
                                       "plugin-domain", "notifications",
                                       "plugin-type",   VALENT_TYPE_NOTIFICATIONS_ADAPTER,
                                       NULL);

      g_object_add_weak_pointer (G_OBJECT (default_listener),
                                 (gpointer)&default_listener);
    }

  return default_listener;
}

/**
 * valent_notifications_get_applications:
 * @notifications: (nullable): a #ValentNotifications
 *
 * Get a dictionary of applications that are known to send notifications.
 *
 * Returns: (transfer none): a #GVariant
 *
 * Since: 1.0
 */
GVariant *
valent_notifications_get_applications (ValentNotifications *notifications)
{
  g_return_val_if_fail (notifications == NULL || VALENT_IS_NOTIFICATIONS (notifications), NULL);

  if (notifications == NULL)
      notifications = valent_notifications_get_default ();

  if (notifications->applications == NULL)
    query_applications (notifications);

  return notifications->applications;
}

