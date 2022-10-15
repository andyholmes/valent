// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-gtk-notifications"

#include "config.h"

#include <time.h>

#include <gio/gdesktopappinfo.h>
#include <gio/gio.h>
#include <libvalent-notifications.h>

#include "valent-gtk-notifications.h"


struct _ValentGtkNotifications
{
  ValentNotificationsAdapter  parent_instance;

  GDBusInterfaceVTable        vtable;
  GDBusNodeInfo              *node_info;
  GDBusInterfaceInfo         *iface_info;
  GDBusConnection            *monitor;
  unsigned int                monitor_id;
  char                       *name_owner;
  unsigned int                name_owner_id;
};

static void   g_async_initable_iface_init (GAsyncInitableIface *iface);

G_DEFINE_TYPE_WITH_CODE (ValentGtkNotifications, valent_gtk_notifications, VALENT_TYPE_NOTIFICATIONS_ADAPTER,
                         G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE, g_async_initable_iface_init))


/*
 * GDBusInterfaceSkeleton
 */
static const char interface_xml[] =
  "<node>"
  "  <interface name='org.gtk.Notifications'>"
  "    <method name='AddNotification'>"
  "      <arg name='applicationId' type='s' direction='in'/>"
  "      <arg name='notificationId' type='s' direction='in'/>"
  "      <arg name='parameters' type='a{sv}' direction='in'/>"
  "    </method>"
  "    <method name='RemoveNotification'>"
  "      <arg name='applicationId' type='s' direction='in'/>"
  "      <arg name='notificationId' type='s' direction='in'/>"
  "    </method>"
  "  </interface>"
  "</node>";

static const char *interface_matches[] = {
  "interface='org.gtk.Notifications',member='AddNotification',type='method_call'",
  "interface='org.gtk.Notifications',member='RemoveNotification',type='method_call'",
  NULL
};


static void
_add_notification (ValentNotificationsAdapter *adapter,
                   GVariant                   *parameters)
{
  g_autoptr (ValentNotification) notification = NULL;
  g_autofree char *desktop_id = NULL;
  g_autoptr (GDesktopAppInfo) desktop_info = NULL;
  g_autoptr (GVariant) props = NULL;
  const char *app_id;
  const char *notif_id;

  /* Extract what we need from the parameters */
  g_variant_get (parameters, "(&s&s@a{sv})", &app_id, &notif_id, &props);

  /* Ignore our own notifications */
  if (g_str_equal (app_id, APPLICATION_ID))
    return;

  /* Deserialize GNotification into ValentNotification */
  notification = valent_notification_deserialize (props);
  valent_notification_set_id (notification, notif_id);

  /* Set a timestamp */
  valent_notification_set_time (notification, valent_timestamp_ms ());

  /* Try and get an application name */
  desktop_id = g_strdup_printf ("%s.desktop", app_id);

  if ((desktop_info = g_desktop_app_info_new (desktop_id)))
    {
      const char *app_name;

      app_name = g_app_info_get_display_name (G_APP_INFO (desktop_info));
      valent_notification_set_application (notification, app_name);
    }

  valent_notifications_adapter_notification_added (adapter, notification);
}

static void
_remove_notification (ValentNotificationsAdapter *adapter,
                      GVariant                   *parameters)
{
  const char *app_id;
  const char *notif_id;

  g_variant_get (parameters, "(&s&s)", &app_id, &notif_id);

  /* Ignore our own notifications */
  if (g_str_equal (app_id, APPLICATION_ID))
    return;

  valent_notifications_adapter_notification_removed (adapter, notif_id);
}

static void
valent_gtk_notifications_method_call (GDBusConnection       *connection,
                                      const char            *sender,
                                      const char            *object_path,
                                      const char            *interface_name,
                                      const char            *method_name,
                                      GVariant              *parameters,
                                      GDBusMethodInvocation *invocation,
                                      gpointer               user_data)
{
  ValentNotificationsAdapter *adapter = VALENT_NOTIFICATIONS_ADAPTER (user_data);
  ValentGtkNotifications *self = VALENT_GTK_NOTIFICATIONS (user_data);
  GDBusMessage *message;
  const char *destination;

  g_assert (VALENT_IS_GTK_NOTIFICATIONS (adapter));

  message = g_dbus_method_invocation_get_message (invocation);
  destination = g_dbus_message_get_destination (message);

  if (g_strcmp0 ("org.gtk.Notifications", destination) != 0 &&
      g_strcmp0 (self->name_owner, destination) != 0)
    goto out;

  if (g_strcmp0 (method_name, "AddNotification") == 0)
    _add_notification (adapter, parameters);

  else if (g_strcmp0 (method_name, "RemoveNotification") == 0)
    _remove_notification (adapter, parameters);

  out:
    g_object_unref (invocation);
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
  ValentGtkNotifications *self = VALENT_GTK_NOTIFICATIONS (user_data);

  g_assert (VALENT_IS_GTK_NOTIFICATIONS (self));

  self->name_owner = g_strdup (name_owner);
}

static void
on_name_vanished (GDBusConnection *connection,
                  const char      *name,
                  gpointer         user_data)
{
  ValentGtkNotifications *self = VALENT_GTK_NOTIFICATIONS (user_data);

  g_assert (VALENT_IS_GTK_NOTIFICATIONS (self));

  g_clear_pointer (&self->name_owner, g_free);
}

static void
become_monitor_cb (GDBusConnection *connection,
                   GAsyncResult    *result,
                   gpointer         user_data)
{
  g_autoptr (GTask) task = G_TASK (user_data);
  ValentGtkNotifications *self = g_task_get_source_object (task);
  g_autoptr (GVariant) reply = NULL;
  g_autoptr (GError) error = NULL;

  reply = g_dbus_connection_call_finish (connection, result, &error);

  if (reply == NULL)
    {
      g_clear_object (&self->monitor);
      g_dbus_error_strip_remote_error (error);
      return g_task_return_error (task, g_steal_pointer (&error));
    }

  g_task_return_boolean (task, TRUE);
}

static void
new_for_address_cb (GObject      *object,
                    GAsyncResult *result,
                    gpointer      user_data)
{
  g_autoptr (GTask) task = G_TASK (user_data);
  ValentGtkNotifications *self = g_task_get_source_object (task);
  GCancellable *cancellable = g_task_get_cancellable (task);
  g_autoptr (GError) error = NULL;

  self->monitor = g_dbus_connection_new_for_address_finish (result, &error);

  if (self->monitor == NULL)
    {
      g_dbus_error_strip_remote_error (error);
      return g_task_return_error (task, g_steal_pointer (&error));
    }

  /* Export the monitor interface */
  self->monitor_id =
    g_dbus_connection_register_object (self->monitor,
                                       "/org/gtk/Notifications",
                                       self->iface_info,
                                       &self->vtable,
                                       self, NULL,
                                       &error);

  if (self->monitor_id == 0)
    {
      g_clear_object (&self->monitor);
      g_dbus_error_strip_remote_error (error);
      return g_task_return_error (task, g_steal_pointer (&error));
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
                          cancellable,
                          (GAsyncReadyCallback)become_monitor_cb,
                          g_steal_pointer (&task));

  /* Watch the true name owner*/
  self->name_owner_id = g_bus_watch_name (G_BUS_TYPE_SESSION,
                                          "org.gtk.Notifications",
                                          G_BUS_NAME_WATCHER_FLAGS_NONE,
                                          on_name_appeared,
                                          on_name_vanished,
                                          self, NULL);
}


/*
 * GAsyncInitable
 */
static void
valent_gtk_notifications_init_async (GAsyncInitable      *initable,
                                     int                  io_priority,
                                     GCancellable        *cancellable,
                                     GAsyncReadyCallback  callback,
                                     gpointer             user_data)
{
  g_autoptr (GTask) task = NULL;
  g_autoptr (GCancellable) destroy = NULL;
  g_autofree char *address = NULL;
  g_autoptr (GError) error = NULL;

  g_assert (VALENT_IS_GTK_NOTIFICATIONS (initable));

  /* Cancel initialization if the object is destroyed */
  destroy = valent_object_attach_cancellable (VALENT_OBJECT (initable),
                                              cancellable);

  task = g_task_new (initable, destroy, callback, user_data);
  g_task_set_priority (task, io_priority);
  g_task_set_source_tag (task, valent_gtk_notifications_init_async);

  /* Get a bus address */
  address = g_dbus_address_get_for_bus_sync (G_BUS_TYPE_SESSION,
                                             destroy,
                                             &error);

  if (address == NULL)
    return g_task_return_error (task, g_steal_pointer (&error));

  /* Get a dedicated connection for monitoring */
  g_dbus_connection_new_for_address (address,
                                     G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT |
                                     G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION,
                                     NULL,
                                     destroy,
                                     (GAsyncReadyCallback)new_for_address_cb,
                                     g_steal_pointer (&task));
}

static void
g_async_initable_iface_init (GAsyncInitableIface *iface)
{
  iface->init_async = valent_gtk_notifications_init_async;
}

/*
 * GObject
 */
static void
valent_gtk_notifications_dispose (GObject *object)
{
  ValentGtkNotifications *self = VALENT_GTK_NOTIFICATIONS (object);

  if (self->name_owner_id > 0)
    {
      g_clear_handle_id (&self->name_owner_id, g_bus_unwatch_name);
      g_clear_pointer (&self->name_owner, g_free);
    }

  if (self->monitor_id != 0)
    {
      g_dbus_connection_unregister_object (self->monitor, self->monitor_id);
      self->monitor_id = 0;
    }

  g_clear_object (&self->monitor);

  G_OBJECT_CLASS (valent_gtk_notifications_parent_class)->dispose (object);
}

static void
valent_gtk_notifications_finalize (GObject *object)
{
  ValentGtkNotifications *self = VALENT_GTK_NOTIFICATIONS (object);

  g_clear_pointer (&self->node_info, g_dbus_node_info_unref);

  G_OBJECT_CLASS (valent_gtk_notifications_parent_class)->finalize(object);
}

static void
valent_gtk_notifications_class_init (ValentGtkNotificationsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = valent_gtk_notifications_dispose;
  object_class->finalize = valent_gtk_notifications_finalize;
}

static void
valent_gtk_notifications_init (ValentGtkNotifications *self)
{
  self->node_info = g_dbus_node_info_new_for_xml (interface_xml, NULL);
  self->iface_info = self->node_info->interfaces[0];

  self->vtable.method_call = valent_gtk_notifications_method_call;
  self->vtable.get_property = NULL;
  self->vtable.set_property = NULL;
}

