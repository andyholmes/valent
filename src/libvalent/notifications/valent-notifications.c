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
 * `ValentNotifications` is an aggregator of notifications, intended for use by
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

static void
on_items_changed (GListModel          *list,
                  unsigned int         position,
                  unsigned int         removed,
                  unsigned int         added,
                  ValentNotifications *self)
{
  g_assert (G_IS_LIST_MODEL (list));
  g_assert (VALENT_IS_NOTIFICATIONS (self));

  for (unsigned int i = 0; i < added; i++)
    {
      g_autoptr (ValentNotification) notification = NULL;

      notification = g_list_model_get_item (list, position + i);
      add_application (self, notification_serialize (notification));
    }
}

/*
 * ValentComponent
 */
static void
valent_notifications_bind_extension (ValentComponent *component,
                                     ValentExtension *extension)
{
  ValentNotifications *self = VALENT_NOTIFICATIONS (component);
  ValentNotificationsAdapter *adapter = VALENT_NOTIFICATIONS_ADAPTER (extension);

  VALENT_ENTRY;

  g_assert (VALENT_IS_NOTIFICATIONS (self));
  g_assert (VALENT_IS_NOTIFICATIONS_ADAPTER (adapter));

  g_signal_connect_object (adapter,
                           "items-changed",
                           G_CALLBACK (on_items_changed),
                           self,
                           G_CONNECT_DEFAULT);

  VALENT_EXIT;
}

static void
valent_notifications_unbind_extension (ValentComponent *component,
                                       ValentExtension *extension)
{
  ValentNotifications *self = VALENT_NOTIFICATIONS (component);
  ValentNotificationsAdapter *adapter = VALENT_NOTIFICATIONS_ADAPTER (extension);

  g_assert (VALENT_IS_NOTIFICATIONS (self));
  g_assert (VALENT_IS_NOTIFICATIONS_ADAPTER (adapter));

  g_signal_handlers_disconnect_by_func (adapter, on_items_changed, self);
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
 * Returns: (transfer none) (not nullable): a `ValentNotifications`
 *
 * Since: 1.0
 */
ValentNotifications *
valent_notifications_get_default (void)
{
  static ValentNotifications *default_instance = NULL;

  if (default_instance == NULL)
    {
      default_instance = g_object_new (VALENT_TYPE_NOTIFICATIONS,
                                       "plugin-domain", "notifications",
                                       "plugin-type",   VALENT_TYPE_NOTIFICATIONS_ADAPTER,
                                       NULL);
      g_object_add_weak_pointer (G_OBJECT (default_instance),
                                 (gpointer)&default_instance);
    }

  return default_instance;
}

/**
 * valent_notifications_get_applications:
 * @notifications: (nullable): a `ValentNotifications`
 *
 * Get a dictionary of applications that are known to send notifications.
 *
 * Returns: (transfer none): a `GVariant`
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

