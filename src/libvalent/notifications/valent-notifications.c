// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-notifications"

#include "config.h"

#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>
#include <libvalent-core.h>

#include "valent-notification.h"
#include "valent-notification-source.h"
#include "valent-notifications.h"


/**
 * SECTION:valent-notifications
 * @short_description: Notification Listener
 * @title: ValentNotifications
 * @stability: Unstable
 * @include: libvalent-notifications.h
 *
 * #ValentNotifications is an aggregator for notification services, with a
 * simple API generally intended to be used by #ValentDevicePlugin
 * implementations.
 *
 * Plugins can provide adapters for services by subclassing the
 * #ValentNotificationSource base class.
 */

struct _ValentNotifications
{
  ValentComponent  parent_instance;

  GVariant        *applications;
};

G_DEFINE_TYPE (ValentNotifications, valent_notifications, VALENT_TYPE_COMPONENT)

enum {
  NOTIFICATION_ADDED,
  NOTIFICATION_REMOVED,
  N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0, };


static ValentNotifications *default_listener = NULL;


static char *
app_info_get_id (GAppInfo *app_info)
{
  const char *desktop_id;
  g_autofree char *ret = NULL;
  const char *filename;
  int l;

  desktop_id = g_app_info_get_id (app_info);
  if (desktop_id != NULL)
    {
      ret = g_strdup (desktop_id);
    }
  else
    {
      filename = g_desktop_app_info_get_filename (G_DESKTOP_APP_INFO (app_info));
      ret = g_path_get_basename (filename);
    }

  if (G_UNLIKELY (g_str_has_suffix (ret, ".desktop") == FALSE))
    return NULL;

  l = strlen (desktop_id);
  *(ret + l - strlen(".desktop")) = '\0';
  return g_steal_pointer (&ret);
}

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
    g_variant_dict_insert_value (&dict, name, application);

  g_clear_pointer (&self->applications, g_variant_unref);
  self->applications = g_variant_ref_sink (g_variant_dict_end (&dict));
}


/*
 * ValentNotificationSource Callbacks
 */
static void
on_notification_added (ValentNotificationSource *source,
                       ValentNotification       *notification,
                       ValentNotifications      *self)
{
  g_assert (VALENT_IS_NOTIFICATION_SOURCE (source));
  g_assert (VALENT_IS_NOTIFICATION (notification));
  g_assert (VALENT_IS_NOTIFICATIONS (self));

  add_application (self, notification_serialize (notification));

  g_signal_emit (G_OBJECT (self), signals [NOTIFICATION_ADDED], 0, notification);
}

static void
on_notification_removed (ValentNotificationSource *source,
                         const char               *id,
                         ValentNotifications      *self)
{
  g_assert (VALENT_IS_NOTIFICATION_SOURCE (source));
  g_assert (id != NULL);
  g_assert (VALENT_IS_NOTIFICATIONS (self));

  g_signal_emit (G_OBJECT (self), signals [NOTIFICATION_REMOVED], 0, id);
}

static void
valent_notification_source_load_cb (ValentNotificationSource *source,
                                    GAsyncResult             *result,
                                    ValentNotifications      *self)
{
  g_autoptr (GError) error = NULL;

  VALENT_ENTRY;

  g_assert (VALENT_IS_NOTIFICATION_SOURCE (source));
  g_assert (g_task_is_valid (result, source));
  g_assert (VALENT_IS_NOTIFICATIONS (self));

  if (!valent_notification_source_load_finish (source, result, &error) &&
      !valent_error_ignore (error))
    g_warning ("%s failed to load: %s", G_OBJECT_TYPE_NAME (source), error->message);

  VALENT_EXIT;
}


/*
 * ValentComponent
 */
static void
valent_notifications_extension_added (ValentComponent *component,
                                      PeasExtension   *extension)
{
  ValentNotifications *self = VALENT_NOTIFICATIONS (component);
  ValentNotificationSource *source = VALENT_NOTIFICATION_SOURCE (extension);

  g_assert (VALENT_IS_NOTIFICATIONS (self));
  g_assert (VALENT_IS_NOTIFICATION_SOURCE (source));

  g_signal_connect_object (source,
                           "notification-added",
                           G_CALLBACK (on_notification_added),
                           self, 0);

  g_signal_connect_object (source,
                           "notification-removed",
                           G_CALLBACK (on_notification_removed),
                           self, 0);

  valent_notification_source_load_async (source,
                                         NULL, // TODO self->cancellable,
                                         (GAsyncReadyCallback)valent_notification_source_load_cb,
                                         self);
}

static void
valent_notifications_extension_removed (ValentComponent *component,
                                        PeasExtension   *extension)
{
  ValentNotifications *self = VALENT_NOTIFICATIONS (component);
  ValentNotificationSource *source = VALENT_NOTIFICATION_SOURCE (extension);

  g_assert (VALENT_IS_NOTIFICATIONS (self));
  g_assert (VALENT_IS_NOTIFICATION_SOURCE (source));

  g_signal_handlers_disconnect_by_func (source, on_notification_added, self);
  g_signal_handlers_disconnect_by_func (source, on_notification_removed, self);
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

  component_class->extension_added = valent_notifications_extension_added;
  component_class->extension_removed = valent_notifications_extension_removed;

  /**
   * ValentNotifications::notification-added:
   * @notifications: a #ValentNotifications
   * @notification: a #ValentNotification
   *
   * #ValentNotifications::notification-removed is emitted when a new notification
   * is added to @notifications.
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
   * #ValentNotifications::notification-removed is emitted when a notification
   * is removed from @notifications.
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
 * valent_notifications_get_applications:
 * @notifications: (nullable): a #ValentNotifications
 *
 * Get a dictionary of applications that are known to send notifications.
 *
 * Returns: (transfer none): a #GVariant
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

/**
 * valent_notifications_get_default:
 *
 * Get the #ValentNotifications singleton.
 *
 * Returns: (transfer none): a #ValentNotifications
 */
ValentNotifications *
valent_notifications_get_default (void)
{
  if (default_listener == NULL)
    {
      default_listener = g_object_new (VALENT_TYPE_NOTIFICATIONS,
                                       "plugin-context", "notifications",
                                       "plugin-type",    VALENT_TYPE_NOTIFICATION_SOURCE,
                                       NULL);

      g_object_add_weak_pointer (G_OBJECT (default_listener), (gpointer) &default_listener);
    }

  return default_listener;
}

