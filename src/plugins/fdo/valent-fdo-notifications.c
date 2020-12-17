// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-fdo-notifications"

#include "config.h"

#include <sys/time.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gio/gdesktopappinfo.h>
#include <libvalent-notifications.h>

#include "valent-fdo-notifications.h"


struct _ValentFdoNotifications
{
  ValentNotificationSource  parent_instance;

  GCancellable             *cancellable;

  GDBusInterfaceVTable      vtable;
  GDBusNodeInfo            *node_info;
  GDBusInterfaceInfo       *iface_info;
  GDBusConnection          *monitor;
  unsigned int              monitor_id;
  GDBusConnection          *session;
  unsigned int              closed_id;
  unsigned int              name_owner_id;
  char                     *name_owner;
};

G_DEFINE_TYPE (ValentFdoNotifications, valent_fdo_notifications, VALENT_TYPE_NOTIFICATION_SOURCE)


/*
 * Map of notification-spec urgency to GNotificationPriority
 *
 * See: https://developer.gnome.org/notification-spec/#urgency-levels
 */
static const unsigned int urgencies[] = {
  G_NOTIFICATION_PRIORITY_LOW,
  G_NOTIFICATION_PRIORITY_NORMAL,
  G_NOTIFICATION_PRIORITY_URGENT
};


/*
 * GDBusInterfaceSkeleton
 */
static const char interface_xml[] =
  "<node>"
  "  <interface name='org.freedesktop.Notifications'>"
  "    <method name='Notify'>"
  "      <arg name='appName' type='s' direction='in'/>"
  "      <arg name='replacesId' type='u' direction='in'/>"
  "      <arg name='iconName' type='s' direction='in'/>"
  "      <arg name='summary' type='s' direction='in'/>"
  "      <arg name='body' type='s' direction='in'/>"
  "      <arg name='actions' type='as' direction='in'/>"
  "      <arg name='hints' type='a{sv}' direction='in'/>"
  "      <arg name='timeout' type='i' direction='in'/>"
  "    </method>"
  "  </interface>"
  "</node>";

static const char *interface_matches[] = {
  "interface='org.freedesktop.Notifications',member='Notify',type='method_call'",
  NULL
};


static GIcon *
_g_icon_new_for_variant (GVariant *variant)
{
  GdkPixbuf *pixbuf;
  gint32 width, height, rowstride;
  gboolean has_alpha;
  gint32 bits_per_sample, n_channels;
  gpointer pixels;

  g_variant_get (variant, "(iiibii^ay)",
                 &width,
                 &height,
                 &rowstride,
                 &has_alpha,
                 &bits_per_sample,
                 &n_channels,
                 &pixels);

  pixbuf = g_object_new (GDK_TYPE_PIXBUF,
                         "bits-per-sample", bits_per_sample,
                         "n-channels",      n_channels,
                         "has-alpha",       has_alpha,
                         "rowstride",       rowstride,
                         "width",           width,
                         "height",          height,
                         "pixels",          pixels,
                         NULL);

  return G_ICON (pixbuf);
}

static void
_notification_closed (ValentNotificationSource *source,
                      GVariant                 *parameters)
{
  unsigned int id, reason;
  g_autofree char *id_str = NULL;

  g_variant_get (parameters, "(uu)", &id, &reason);

  id_str = g_strdup_printf ("%u", id);
  valent_notification_source_emit_notification_removed (source, id_str);
}

static void
_notify (ValentNotificationSource *source,
         GVariant                 *parameters)
{
  g_autoptr (ValentNotification) notification = NULL;
  g_autoptr (GIcon) icon = NULL;

  const char *app_name;
  guint32 replaces_id;
  const char *app_icon;
  const char *summary;
  const char *body;
  GVariant *actions;
  GVariant *hints;
  gint32 expire_timeout;

  g_autofree char *replaces_id_str = NULL;
  g_autoptr (GVariant) image_data = NULL;
  const char *image_path;
  guchar urgency;

  /* Extract what we need from the parameters */
  g_variant_get (parameters, "(&su&s&s&s@as@a{sv}i)",
                 &app_name,
                 &replaces_id,
                 &app_icon,
                 &summary,
                 &body,
                 &actions,
                 &hints,
                 &expire_timeout);

  replaces_id_str = g_strdup_printf ("%u", replaces_id);

  /* Deserialize GNotification into ValentNotification */
  notification = valent_notification_new (NULL);
  valent_notification_set_id (notification, replaces_id_str);
  valent_notification_set_application (notification, app_name);
  valent_notification_set_title (notification, summary);
  valent_notification_set_body (notification, body);

  /* Notification Icon */
  if (g_variant_lookup (hints, "image-data", "@(iiibiiay)", &image_data) ||
      g_variant_lookup (hints, "image_data", "@(iiibiiay)", &image_data))
    {
      icon = _g_icon_new_for_variant (image_data);
      valent_notification_set_icon (notification, icon);
    }
  else if (g_variant_lookup (hints, "image-path", "&s", &image_path) ||
           g_variant_lookup (hints, "image_path", "&s", &image_path))
    {
      icon = g_icon_new_for_string (image_path, NULL);
      valent_notification_set_icon (notification, icon);
    }
  else if (app_icon[0] != '\0')
    {
      icon = g_icon_new_for_string (app_icon, NULL);
      valent_notification_set_icon (notification, icon);
    }
  else if (g_variant_lookup (hints, "icon_data", "@(iiibiiay)", &image_data))
    {
      icon = _g_icon_new_for_variant (image_data);
      valent_notification_set_icon (notification, icon);
    }

  /* Map libnotify urgency to GNotification priority */
  if (g_variant_lookup (hints, "urgency", "y", &urgency) && urgency < G_N_ELEMENTS (urgencies))
    valent_notification_set_priority (notification, urgencies[urgency]);
  else
    valent_notification_set_priority (notification, G_NOTIFICATION_PRIORITY_NORMAL);

  /* Set a timestamp */
  valent_notification_set_time (notification, valent_timestamp_ms ());

  valent_notification_source_emit_notification_added (source, notification);
}

static void
valent_fdo_notifications_method_call (GDBusConnection       *connection,
                                      const char            *sender,
                                      const char            *object_path,
                                      const char            *interface_name,
                                      const char            *method_name,
                                      GVariant              *parameters,
                                      GDBusMethodInvocation *invocation,
                                      gpointer               user_data)
{
  ValentNotificationSource *source = VALENT_NOTIFICATION_SOURCE (user_data);
  ValentFdoNotifications *self = VALENT_FDO_NOTIFICATIONS (user_data);
  GDBusMessage *message;
  const char *destination;

  g_assert (VALENT_IS_NOTIFICATION_SOURCE (source));
  g_assert (VALENT_IS_FDO_NOTIFICATIONS (self));

  message = g_dbus_method_invocation_get_message (invocation);
  destination = g_dbus_message_get_destination (message);

  if G_UNLIKELY (g_strcmp0 (self->name_owner, destination))
    goto out;

  if (g_strcmp0 (method_name, "Notify") == 0)
    _notify (source, parameters);

  out:
    g_object_unref (invocation);
}

static void
on_notification_closed (GDBusConnection *connection,
                        const char      *sender_name,
                        const char      *object_path,
                        const char      *interface_name,
                        const char      *signal_name,
                        GVariant        *parameters,
                        gpointer         user_data)
{
  ValentNotificationSource *source = VALENT_NOTIFICATION_SOURCE (user_data);

  g_assert (VALENT_IS_NOTIFICATION_SOURCE (source));
  g_assert (g_strcmp0 (signal_name, "NotificationClosed") == 0);

  _notification_closed (source, parameters);
}

/*
 * Setup
 */
static void
on_name_appeared (GDBusConnection *connection,
                  const char      *name,
                  const char      *name_owner,
                  gpointer         user_data)
{
  ValentFdoNotifications *self = VALENT_FDO_NOTIFICATIONS (user_data);

  g_set_object (&self->session, connection);
  self->name_owner = g_strdup (name_owner);

  if (self->closed_id == 0)
    {
      self->closed_id =
        g_dbus_connection_signal_subscribe (connection,
                                            "org.freedesktop.Notifications",
                                            "org.freedesktop.Notifications",
                                            "NotificationClosed",
                                            "/org/freedesktop/Notifications",
                                            NULL,
                                            G_DBUS_SIGNAL_FLAGS_NONE,
                                            on_notification_closed,
                                            g_object_ref (self),
                                            g_object_unref);
    }
}

static void
on_name_vanished (GDBusConnection *connection,
                  const char      *name,
                  gpointer         user_data)
{
  ValentFdoNotifications *self = VALENT_FDO_NOTIFICATIONS (user_data);

  g_set_object (&self->session, connection);
  g_clear_pointer (&self->name_owner, g_free);

  if (self->closed_id > 0)
    {
      g_dbus_connection_signal_unsubscribe (connection, self->closed_id);
      self->closed_id = 0;
    }
}

static void
become_monitor_cb (GDBusConnection *connection,
                   GAsyncResult    *result,
                   gpointer         user_data)
{
  g_autoptr (GTask) task = G_TASK (user_data);
  ValentFdoNotifications *self = g_task_get_source_object (task);
  GError *error = NULL;
  g_autoptr (GVariant) reply = NULL;

  reply = g_dbus_connection_call_finish (connection, result, &error);

  if (reply == NULL)
    {
      g_clear_object (&self->monitor);
      g_dbus_error_strip_remote_error (error);
      return g_task_return_error (task, error);
    }

  g_task_return_boolean (task, TRUE);
}

static void
new_for_address_cb (GObject      *object,
                    GAsyncResult *result,
                    gpointer      user_data)
{
  g_autoptr (GTask) task = G_TASK (user_data);
  ValentFdoNotifications *self = g_task_get_source_object (task);
  GError *error = NULL;

  self->monitor = g_dbus_connection_new_for_address_finish (result, &error);

  if (self->monitor == NULL)
    {
      g_dbus_error_strip_remote_error (error);
      return g_task_return_error (task, error);
    }

  /* Export the monitor interface */
  self->monitor_id =
    g_dbus_connection_register_object (self->monitor,
                                       "/org/freedesktop/Notifications",
                                       self->iface_info,
                                       &self->vtable,
                                       self, NULL,
                                       &error);

  if (self->monitor_id == 0)
    {
      g_clear_object (&self->monitor);
      g_dbus_error_strip_remote_error (error);
      return g_task_return_error (task, error);
    }

  /* Become a monitor for notifications */
  g_dbus_connection_call (self->monitor,
                          "org.freedesktop.DBus",
                          "/org/freedesktop/DBus",
                          "org.freedesktop.DBus.Monitoring",
                          "BecomeMonitor",
                          g_variant_new ("(^asu)", interface_matches, 0),
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          self->cancellable,
                          (GAsyncReadyCallback)become_monitor_cb,
                          g_steal_pointer (&task));

  /* Watch the true name owner */
  self->name_owner_id = g_bus_watch_name (G_BUS_TYPE_SESSION,
                                          "org.freedesktop.Notifications",
                                          G_BUS_NAME_WATCHER_FLAGS_NONE,
                                          on_name_appeared,
                                          on_name_vanished,
                                          self, NULL);
}


/*
 * ValentNotificationSource
 */
static void
valent_fdo_notifications_load_async (ValentNotificationSource *source,
                                     GCancellable             *cancellable,
                                     GAsyncReadyCallback       callback,
                                     gpointer                  user_data)
{
  ValentFdoNotifications *self = VALENT_FDO_NOTIFICATIONS (source);
  g_autoptr (GTask) task = NULL;
  GError *error = NULL;
  g_autofree char *address = NULL;

  g_assert (VALENT_IS_FDO_NOTIFICATIONS (self));

  if (cancellable != NULL)
    g_signal_connect_object (cancellable,
                             "cancelled",
                             G_CALLBACK (g_cancellable_cancel),
                             self->cancellable,
                             G_CONNECT_SWAPPED);

  task = g_task_new (source, self->cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_fdo_notifications_load_async);

  /* Get a bus address */
  address = g_dbus_address_get_for_bus_sync (G_BUS_TYPE_SESSION,
                                             self->cancellable,
                                             &error);

  if (address == NULL)
    return g_task_return_error (task, error);

  /* Get a dedicated connection for monitoring */
  g_dbus_connection_new_for_address (address,
                                     G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT |
                                     G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION,
                                     NULL,
                                     self->cancellable,
                                     (GAsyncReadyCallback)new_for_address_cb,
                                     g_steal_pointer (&task));
}

/*
 * GObject
 */
static void
valent_fdo_notifications_dispose (GObject *object)
{
  ValentFdoNotifications *self = VALENT_FDO_NOTIFICATIONS (object);

  if (!g_cancellable_is_cancelled (self->cancellable))
    g_cancellable_cancel (self->cancellable);

  if (self->name_owner_id > 0)
    {
      g_clear_handle_id (&self->name_owner_id, g_bus_unwatch_name);
      g_clear_pointer (&self->name_owner, g_free);
      g_clear_object (&self->session);
    }

  if (self->closed_id > 0)
    {
      g_dbus_connection_signal_unsubscribe (self->session, self->closed_id);
      self->closed_id = 0;
    }

  if (self->monitor_id != 0)
    {
      g_dbus_connection_unregister_object (self->monitor, self->monitor_id);
      self->monitor_id = 0;
      g_clear_object (&self->monitor);
    }

  G_OBJECT_CLASS (valent_fdo_notifications_parent_class)->dispose (object);
}

static void
valent_fdo_notifications_finalize (GObject *object)
{
  ValentFdoNotifications *self = VALENT_FDO_NOTIFICATIONS (object);

  g_clear_object (&self->cancellable);
  g_clear_pointer (&self->node_info, g_dbus_node_info_unref);

  G_OBJECT_CLASS (valent_fdo_notifications_parent_class)->finalize (object);
}

static void
valent_fdo_notifications_class_init (ValentFdoNotificationsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentNotificationSourceClass *source_class = VALENT_NOTIFICATION_SOURCE_CLASS (klass);

  object_class->dispose = valent_fdo_notifications_dispose;
  object_class->finalize = valent_fdo_notifications_finalize;

  source_class->load_async = valent_fdo_notifications_load_async;
}

static void
valent_fdo_notifications_init (ValentFdoNotifications *self)
{
  self->cancellable = g_cancellable_new ();
  self->node_info = g_dbus_node_info_new_for_xml (interface_xml, NULL);
  self->iface_info = self->node_info->interfaces[0];

  self->vtable.method_call = valent_fdo_notifications_method_call;
  self->vtable.get_property = NULL;
  self->vtable.set_property = NULL;
}

