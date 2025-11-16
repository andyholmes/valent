// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-fdo-notifications"

#include "config.h"

#include <gio/gio.h>
#include <valent.h>

#ifdef HAVE_GLYCIN
#include <glycin.h>
#endif /* HAVE_GLYCIN */

#include "valent-fdo-notifications.h"


struct _ValentFdoNotifications
{
  ValentNotificationsAdapter  parent_instance;

  GHashTable                 *active;
  GHashTable                 *pending;
  GDBusConnection            *monitor;
  unsigned int                monitor_id;
  char                       *name_owner;
  unsigned int                name_owner_id;
  unsigned int                filter_id;
};

static void   g_async_initable_iface_init (GAsyncInitableIface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (ValentFdoNotifications, valent_fdo_notifications, VALENT_TYPE_NOTIFICATIONS_ADAPTER,
                               G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE, g_async_initable_iface_init))


/*
 * Map of notification-spec urgency to GNotificationPriority
 *
 * See: https://developer-old.gnome.org/notification-spec/#urgency-levels
 */
static const unsigned int urgencies[] = {
  G_NOTIFICATION_PRIORITY_LOW,
  G_NOTIFICATION_PRIORITY_NORMAL,
  G_NOTIFICATION_PRIORITY_URGENT
};


/*
 * GDBusInterfaceSkeleton
 */
static const char *interface_matches[] = {
  "interface='org.freedesktop.Notifications',member='Notify',type='method_call'",
  "interface='org.freedesktop.Notifications',member='NotificationClosed',type='signal'",
  "type='method_return'",
  NULL
};


static GIcon *
_g_icon_new_for_variant (GVariant *image_data)
{
#ifdef HAVE_GLYCIN
  int32_t width, height, rowstride;
  gboolean has_alpha;
  int32_t bits_per_sample, n_channels;
  g_autoptr (GVariant) data_variant = NULL;
  g_autoptr (GlyCreator) creator = NULL;
  g_autoptr (GlyNewFrame) frame = NULL;
  g_autoptr (GlyEncodedImage) image = NULL;
  g_autoptr (GBytes) texture = NULL;
  g_autoptr (GBytes) encoded = NULL;
  g_autoptr (GError) error = NULL;

  g_variant_get (image_data, "(iiibii@ay)",
                 &width,
                 &height,
                 &rowstride,
                 &has_alpha,
                 &bits_per_sample,
                 &n_channels,
                 &data_variant);
  texture = g_bytes_new (g_variant_get_data (data_variant),
                         g_variant_get_size (data_variant));

  creator = gly_creator_new ("image/png", &error);
  if (creator == NULL)
    {
      g_warning ("%s(): Creating image: %s", G_STRFUNC, error->message);
      return NULL;
    }

  frame = gly_creator_add_frame_with_stride (creator,
                                             width,
                                             height,
                                             rowstride,
                                             has_alpha
                                               ? GLY_MEMORY_R8G8B8A8
                                               : GLY_MEMORY_R8G8B8,
                                             texture,
                                             &error);
  if (frame == NULL)
    {
      g_warning ("%s(): Adding frame: %s", G_STRFUNC, error->message);
      return NULL;
    }

  image = gly_creator_create (creator, &error);
  if (image == NULL)
    {
      g_warning ("%s(): Encoding image: %s", G_STRFUNC, error->message);
      return NULL;
    }

  encoded = gly_encoded_image_get_data (image);

  return g_bytes_icon_new (encoded);
#else
  return NULL;
#endif /* HAVE_GLYCIN */
}

static gboolean
valent_fdo_notifications_notify_main (gpointer data)
{
  GTask *task = G_TASK (data);
  ValentFdoNotifications *self = g_task_get_source_object (task);
  GDBusMessage *message = g_task_get_task_data (task);
  g_autoptr (ValentNotification) notification = NULL;
  GVariant *parameters;
  const char *app_name;
  uint32_t replaces_id;
  const char *app_icon;
  const char *summary;
  const char *body;
  g_autoptr (GVariant) actions = NULL;
  g_autoptr (GVariant) hints = NULL;
  int32_t expire_timeout;
  g_autoptr (GVariant) image_data = NULL;
  const char *image_path;
  g_autoptr (GIcon) icon = NULL;
  unsigned char urgency;
  g_autofree char *id_str = NULL;
  uint32_t serial;

  g_assert (VALENT_IS_MAIN_THREAD ());

  parameters = g_dbus_message_get_body (message);
  g_variant_get (parameters, "(&su&s&s&s@as@a{sv}i)",
                 &app_name,
                 &replaces_id,
                 &app_icon,
                 &summary,
                 &body,
                 &actions,
                 &hints,
                 &expire_timeout);

  /* The notification ID hasn't been received yet,
   * so use `replaces_id` for the new notification
   */
  id_str = g_strdup_printf ("%u", replaces_id);

  notification = valent_notification_new (NULL);
  valent_notification_set_id (notification, id_str);
  valent_notification_set_application (notification, app_name);
  valent_notification_set_title (notification, summary);
  valent_notification_set_body (notification, body);

  /* This ordering is required by the specification.
   * See: https://specifications.freedesktop.org/notification-spec/latest/icons-and-images.html
   */
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
  else if (*app_icon != '\0')
    {
      icon = g_icon_new_for_string (app_icon, NULL);
      valent_notification_set_icon (notification, icon);
    }
  else if (g_variant_lookup (hints, "icon_data", "@(iiibiiay)", &image_data))
    {
      icon = _g_icon_new_for_variant (image_data);
      valent_notification_set_icon (notification, icon);
    }

  /* Map libnotify urgency to GNotification priority
   */
  if (g_variant_lookup (hints, "urgency", "y", &urgency) && urgency < G_N_ELEMENTS (urgencies))
    valent_notification_set_priority (notification, urgencies[urgency]);
  else
    valent_notification_set_priority (notification, G_NOTIFICATION_PRIORITY_NORMAL);

  valent_notification_set_time (notification, valent_timestamp_ms ());

  /* Wait for the reply to get the correct notification ID
   */
  serial = g_dbus_message_get_serial (message);
  g_hash_table_replace (self->pending,
                        GUINT_TO_POINTER (serial),
                        g_object_ref (notification));
  g_task_return_boolean (task, TRUE);

  return G_SOURCE_REMOVE;
}

static gboolean
valent_fdo_notifications_notify_return_main (gpointer data)
{
  GTask *task = G_TASK (data);
  ValentFdoNotifications *self = g_task_get_source_object (task);
  ValentNotificationsAdapter *adapter = g_task_get_source_object (task);
  GDBusMessage *message = g_task_get_task_data (task);
  g_autoptr (ValentNotification) notification = NULL;
  uint32_t reply_serial;
  GVariant *body = NULL;
  uint32_t id;
  g_autofree char *id_str = NULL;

  g_assert (VALENT_IS_MAIN_THREAD ());

  reply_serial = g_dbus_message_get_reply_serial (message);
  if (!g_hash_table_steal_extended (self->pending,
                                    GUINT_TO_POINTER (reply_serial),
                                    NULL,
                                    (void **)&notification))
    {
      g_task_return_boolean (task, TRUE);
      return G_SOURCE_REMOVE;
    }

  body = g_dbus_message_get_body (message);
  g_variant_get (body, "(u)", &id);
  id_str = g_strdup_printf ("%u", id);
  valent_notification_set_id (notification, id_str);

  g_hash_table_replace (self->active,
                        (char *)valent_notification_get_id (notification),
                        g_object_ref (notification));
  valent_notifications_adapter_notification_added (adapter, notification);

  g_task_return_boolean (task, TRUE);

  return G_SOURCE_REMOVE;
}

static gboolean
valent_fdo_notifications_notification_closed_main (gpointer data)
{
  GTask *task = G_TASK (data);
  ValentFdoNotifications *self = g_task_get_source_object (task);
  ValentNotificationsAdapter *adapter = g_task_get_source_object (task);
  GDBusMessage *message = g_task_get_task_data (task);
  g_autoptr (ValentNotification) notification = NULL;
  uint32_t id, reason;
  GVariant *body = NULL;
  g_autofree char *id_str = NULL;

  g_assert (VALENT_IS_MAIN_THREAD ());

  body = g_dbus_message_get_body (message);
  g_variant_get (body, "(uu)", &id, &reason);
  id_str = g_strdup_printf ("%u", id);

  if (g_hash_table_steal_extended (self->active,
                                   id_str,
                                   NULL,
                                   (void **)&notification))
    {
      valent_notifications_adapter_notification_removed (adapter, notification);
    }

  g_task_return_boolean (task, TRUE);

  return G_SOURCE_REMOVE;
}

static GDBusMessage *
valent_fdo_notifications_filter (GDBusConnection *connection,
                                 GDBusMessage    *message,
                                 gboolean         incoming,
                                 gpointer         user_data)
{
  ValentFdoNotifications *self = VALENT_FDO_NOTIFICATIONS (user_data);
  GDBusMessageType message_type = G_DBUS_MESSAGE_TYPE_INVALID;
  GSourceFunc dispatch_func = NULL;

  g_assert (VALENT_IS_FDO_NOTIFICATIONS (self));

  message_type = g_dbus_message_get_message_type (message);
  if (message_type == G_DBUS_MESSAGE_TYPE_METHOD_CALL)
    {
      const char *member;

      member = g_dbus_message_get_member (message);
      if (g_strcmp0 (member, "Notify") == 0)
        {
          const char *destination;

          valent_object_lock (VALENT_OBJECT (self));
          destination = g_dbus_message_get_destination (message);
          if (g_strcmp0 (destination, "org.freedesktop.Notifications") == 0 ||
              g_strcmp0 (destination, self->name_owner) == 0)
            {
              dispatch_func = valent_fdo_notifications_notify_main;
            }
          valent_object_unlock (VALENT_OBJECT (self));
        }
    }
  else if (message_type == G_DBUS_MESSAGE_TYPE_METHOD_RETURN)
    {
      const char *signature;

      /* NOTE: The method return will have no member name, but checking
       *       the signature avoids unnecessary locking
       */
      signature = g_dbus_message_get_signature (message);
      if (g_str_equal (signature, "u"))
        {
          const char *sender = NULL;

          valent_object_lock (VALENT_OBJECT (self));
          sender = g_dbus_message_get_sender (message);
          if (g_strcmp0 (sender, "org.freedesktop.Notifications") == 0 ||
              g_strcmp0 (sender, self->name_owner) == 0)
            {
              dispatch_func = valent_fdo_notifications_notify_return_main;
            }
          valent_object_unlock (VALENT_OBJECT (self));
        }
    }
  else if (message_type == G_DBUS_MESSAGE_TYPE_SIGNAL)
    {
      const char *member;

      /* NOTE: If a proxy daemon is running (i.e. GNOME Shell),
       *       NotificationClosed is not re-emitted by the name owner
       */
      member = g_dbus_message_get_member (message);
      if (g_strcmp0 (member, "NotificationClosed") == 0)
        {
          g_autoptr (GTask) task = NULL;

          task = g_task_new (self, NULL, NULL, NULL);
          g_task_set_source_tag (task, valent_fdo_notifications_notification_closed_main);
          g_task_set_task_data (task, g_object_ref (message), g_object_unref);

          g_idle_add_full (g_task_get_priority (task),
                           valent_fdo_notifications_notification_closed_main,
                           g_object_ref (task),
                           g_object_unref);
        }
    }

  if (dispatch_func != NULL)
    {
      g_autoptr (GTask) task = NULL;

      task = g_task_new (self, NULL, NULL, NULL);
      g_task_set_source_tag (task, dispatch_func);
      g_task_set_task_data (task, g_object_ref (message), g_object_unref);

      g_idle_add_full (g_task_get_priority (task),
                       dispatch_func,
                       g_object_ref (task),
                       g_object_unref);
    }

  g_object_unref (message);
  return NULL;
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

  valent_object_lock (VALENT_OBJECT (self));
  g_set_str (&self->name_owner, name_owner);
  valent_object_unlock (VALENT_OBJECT (self));

  if (self->filter_id == 0)
    {
      self->filter_id =
        g_dbus_connection_add_filter (self->monitor,
                                      valent_fdo_notifications_filter,
                                      g_object_ref (self),
                                      g_object_unref);
    }

  valent_extension_plugin_state_changed (VALENT_EXTENSION (self),
                                         VALENT_PLUGIN_STATE_ACTIVE,
                                         NULL);
}

static void
on_name_vanished (GDBusConnection *connection,
                  const char      *name,
                  gpointer         user_data)
{
  ValentFdoNotifications *self = VALENT_FDO_NOTIFICATIONS (user_data);

  valent_object_lock (VALENT_OBJECT (self));
  g_clear_pointer (&self->name_owner, g_free);
  valent_object_unlock (VALENT_OBJECT (self));

  if (self->filter_id > 0)
    {
      g_dbus_connection_remove_filter (self->monitor, self->filter_id);
      self->filter_id = 0;
    }

  valent_extension_plugin_state_changed (VALENT_EXTENSION (self),
                                         VALENT_PLUGIN_STATE_INACTIVE,
                                         NULL);
}

static void
become_monitor_cb (GDBusConnection *connection,
                   GAsyncResult    *result,
                   gpointer         user_data)
{
  g_autoptr (GTask) task = G_TASK (user_data);
  ValentFdoNotifications *self = g_task_get_source_object (task);
  g_autoptr (GVariant) reply = NULL;
  g_autoptr (GError) error = NULL;

  reply = g_dbus_connection_call_finish (connection, result, &error);
  if (reply == NULL)
    {
      valent_extension_plugin_state_changed (VALENT_EXTENSION (self),
                                             VALENT_PLUGIN_STATE_ERROR,
                                             error);
      g_clear_object (&self->monitor);
      g_dbus_error_strip_remote_error (error);
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  self->name_owner_id = g_bus_watch_name (G_BUS_TYPE_SESSION,
                                          "org.freedesktop.Notifications",
                                          G_BUS_NAME_WATCHER_FLAGS_NONE,
                                          on_name_appeared,
                                          on_name_vanished,
                                          self, NULL);

  g_task_return_boolean (task, TRUE);
}

static void
new_for_address_cb (GObject      *object,
                    GAsyncResult *result,
                    gpointer      user_data)
{
  g_autoptr (GTask) task = G_TASK (user_data);
  ValentFdoNotifications *self = g_task_get_source_object (task);
  GCancellable *cancellable = g_task_get_cancellable (task);
  g_autoptr (GError) error = NULL;

  self->monitor = g_dbus_connection_new_for_address_finish (result, &error);
  if (self->monitor == NULL)
    {
      valent_extension_plugin_state_changed (VALENT_EXTENSION (self),
                                             VALENT_PLUGIN_STATE_ERROR,
                                             error);
      g_dbus_error_strip_remote_error (error);
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  /* Register as a monitor
   */
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
}


/*
 * GAsyncInitable
 */
static void
valent_fdo_notifications_init_async (GAsyncInitable             *initable,
                                     int                         io_priority,
                                     GCancellable               *cancellable,
                                     GAsyncReadyCallback         callback,
                                     gpointer                    user_data)
{
  g_autoptr (GTask) task = NULL;
  g_autofree char *address = NULL;
  g_autoptr (GError) error = NULL;

  g_assert (VALENT_IS_FDO_NOTIFICATIONS (initable));

  task = g_task_new (initable, cancellable, callback, user_data);
  g_task_set_priority (task, io_priority);
  g_task_set_source_tag (task, valent_fdo_notifications_init_async);

  /* Get a bus address and dedicated monitoring connection
   */
  address = g_dbus_address_get_for_bus_sync (G_BUS_TYPE_SESSION,
                                             cancellable,
                                             &error);
  if (address == NULL)
    {
      valent_extension_plugin_state_changed (VALENT_EXTENSION (initable),
                                             VALENT_PLUGIN_STATE_ERROR,
                                             error);
      g_dbus_error_strip_remote_error (error);
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  g_dbus_connection_new_for_address (address,
                                     G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT |
                                     G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION,
                                     NULL,
                                     cancellable,
                                     (GAsyncReadyCallback)new_for_address_cb,
                                     g_steal_pointer (&task));
}

static void
g_async_initable_iface_init (GAsyncInitableIface *iface)
{
  iface->init_async = valent_fdo_notifications_init_async;
}

/*
 * ValentObject
 */
static void
valent_fdo_notifications_destroy (ValentObject *object)
{
  ValentFdoNotifications *self = VALENT_FDO_NOTIFICATIONS (object);

  if (self->filter_id > 0)
    {
      g_dbus_connection_remove_filter (self->monitor, self->filter_id);
      self->filter_id = 0;
    }

  if (self->name_owner_id > 0)
    {
      g_clear_handle_id (&self->name_owner_id, g_bus_unwatch_name);
      valent_object_lock (VALENT_OBJECT (self));
      g_clear_pointer (&self->name_owner, g_free);
      valent_object_unlock (VALENT_OBJECT (self));
    }

  if (self->monitor_id != 0)
    {
      g_dbus_connection_unregister_object (self->monitor, self->monitor_id);
      self->monitor_id = 0;
    }

  g_clear_object (&self->monitor);

  VALENT_OBJECT_CLASS (valent_fdo_notifications_parent_class)->destroy (object);
}

/*
 * GObject
 */
static void
valent_fdo_notifications_finalize (GObject *object)
{
  ValentFdoNotifications *self = VALENT_FDO_NOTIFICATIONS (object);

  g_clear_pointer (&self->active, g_hash_table_unref);
  g_clear_pointer (&self->pending, g_hash_table_unref);

  G_OBJECT_CLASS (valent_fdo_notifications_parent_class)->finalize (object);
}

static void
valent_fdo_notifications_class_init (ValentFdoNotificationsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentObjectClass *vobject_class = VALENT_OBJECT_CLASS (klass);

  object_class->finalize = valent_fdo_notifications_finalize;

  vobject_class->destroy = valent_fdo_notifications_destroy;
}

static void
valent_fdo_notifications_init (ValentFdoNotifications *self)
{
  self->active = g_hash_table_new_full (g_str_hash,
                                        g_str_equal,
                                        NULL,
                                        g_object_unref);
  self->pending = g_hash_table_new_full (NULL,
                                         NULL,
                                         NULL,
                                         g_object_unref);
}

