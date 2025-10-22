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

  GHashTable                 *pending;
  GDBusInterfaceVTable        vtable;
  GDBusNodeInfo              *node_info;
  GDBusInterfaceInfo         *iface_info;
  GDBusConnection            *monitor;
  unsigned int                monitor_id;
  char                       *name_owner;
  unsigned int                name_owner_id;
  GDBusConnection            *session;
  unsigned int                closed_id;
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
  "      <arg name='id' type='u' direction='out'/>"
  "    </method>"
  "  </interface>"
  "</node>";

static const char *interface_matches[] = {
  "interface='org.freedesktop.Notifications',member='Notify',type='method_call'",
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
  const unsigned char *data = NULL;
  size_t data_len = 0;
  size_t expected_len = 0;

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

  data = g_variant_get_data (data_variant);
  data_len = g_variant_get_size (data_variant);
  expected_len = (height - 1) * rowstride + width
    * ((n_channels * bits_per_sample + 7) / 8);

  if (expected_len != data_len)
    {
      g_warning ("Expected image data to be of length %zu not %zu",
                 expected_len,
                 data_len);
      return NULL;
    }

  g_return_val_if_fail (bits_per_sample == 8, NULL);
  g_return_val_if_fail ((has_alpha && n_channels == 4) ||
                        (!has_alpha && n_channels == 3), NULL);

  creator = gly_creator_new ("image/png", &error);
  if (creator == NULL)
    {
      g_warning ("%s(): Creating image: %s", G_STRFUNC, error->message);
      return NULL;
    }

  texture = g_bytes_new (data, data_len);
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

static void
_notification_closed (ValentNotificationsAdapter *adapter,
                      GVariant                   *parameters)
{
  uint32_t id, reason;
  g_autofree char *id_str = NULL;

  g_variant_get (parameters, "(uu)", &id, &reason);

  id_str = g_strdup_printf ("%u", id);
  valent_notifications_adapter_notification_removed (adapter, id_str);
}

static void
_notify (ValentNotificationsAdapter *adapter,
         GDBusMessage               *message,
         GVariant                   *parameters)
{
  ValentFdoNotifications *self = VALENT_FDO_NOTIFICATIONS (adapter);
  g_autoptr (ValentNotification) notification = NULL;
  g_autoptr (GIcon) icon = NULL;
  uint32_t serial;

  const char *app_name;
  uint32_t replaces_id;
  const char *app_icon;
  const char *summary;
  const char *body;
  g_autoptr (GVariant) actions = NULL;
  g_autoptr (GVariant) hints = NULL;
  int32_t expire_timeout;

  g_autofree char *replaces_id_str = NULL;
  g_autoptr (GVariant) image_data = NULL;
  const char *image_path;
  unsigned char urgency;

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
  valent_resource_set_title (VALENT_RESOURCE (notification), summary);
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

  /* Map libnotify urgency to GNotification priority */
  if (g_variant_lookup (hints, "urgency", "y", &urgency) && urgency < G_N_ELEMENTS (urgencies))
    valent_notification_set_priority (notification, urgencies[urgency]);
  else
    valent_notification_set_priority (notification, G_NOTIFICATION_PRIORITY_NORMAL);

  /* Set a timestamp */
  valent_notification_set_time (notification, valent_timestamp_ms ());

  /* Wait for the reply to get the correct notification ID
   */
  valent_object_lock (VALENT_OBJECT (self));
  serial = g_dbus_message_get_serial (message);
  g_hash_table_replace (self->pending,
                        GUINT_TO_POINTER (serial),
                        g_object_ref (notification));
  valent_object_unlock (VALENT_OBJECT (self));
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
  ValentNotificationsAdapter *adapter = VALENT_NOTIFICATIONS_ADAPTER (user_data);
  ValentFdoNotifications *self = VALENT_FDO_NOTIFICATIONS (user_data);
  GDBusMessage *message;
  const char *destination;

  g_assert (VALENT_IS_NOTIFICATIONS_ADAPTER (adapter));
  g_assert (VALENT_IS_FDO_NOTIFICATIONS (self));

  message = g_dbus_method_invocation_get_message (invocation);
  destination = g_dbus_message_get_destination (message);

  // TODO: accepting notifications from the well-known name causes duplicates on
  //       GNOME Shell where a proxy daemon is run.
  if (g_strcmp0 ("org.freedesktop.Notifications", destination) != 0 &&
      g_strcmp0 (self->name_owner, destination) != 0)
    goto out;

  if (g_strcmp0 (method_name, "Notify") == 0)
    _notify (adapter, message, parameters);

  out:
    g_object_unref (invocation);
}

static gboolean
valent_fdo_notifications_filter_main (gpointer data)
{
  GTask *task = G_TASK (data);
  ValentFdoNotifications *self = g_task_get_source_object (task);
  ValentNotificationsAdapter *adapter = g_task_get_source_object (task);
  GDBusMessage *message = g_task_get_task_data (task);
  g_autoptr (ValentNotification) notification = NULL;
  uint32_t reply_serial;

  g_assert (VALENT_IS_MAIN_THREAD ());

  reply_serial = g_dbus_message_get_reply_serial (message);
  valent_object_lock (VALENT_OBJECT (self));
  g_hash_table_steal_extended (self->pending,
                               GUINT_TO_POINTER (reply_serial),
                               NULL,
                               (void **)&notification);
  valent_object_unlock (VALENT_OBJECT (self));

  if (notification != NULL)
    {
      GVariant *body = NULL;

      body = g_dbus_message_get_body (message);
      if (g_variant_is_of_type (body, G_VARIANT_TYPE ("(u)")))
        {
          g_autofree char *id_str = NULL;
          uint32_t notification_id;

          g_variant_get (body, "(u)", &notification_id);
          id_str = g_strdup_printf ("%u", notification_id);
          valent_notification_set_id (notification, id_str);
        }

      valent_notifications_adapter_notification_added (adapter, notification);
    }

  return G_SOURCE_REMOVE;
}

static GDBusMessage *
valent_fdo_notifications_filter (GDBusConnection *connection,
                                 GDBusMessage    *message,
                                 gboolean         incoming,
                                 gpointer         user_data)
{
  ValentFdoNotifications *self = VALENT_FDO_NOTIFICATIONS (user_data);
  GDBusMessageType message_type;

  g_assert (VALENT_IS_FDO_NOTIFICATIONS (self));

  message_type = g_dbus_message_get_message_type (message);
  if (message_type == G_DBUS_MESSAGE_TYPE_METHOD_RETURN)
    {
      uint32_t reply_serial;
      gboolean pending;

      reply_serial = g_dbus_message_get_reply_serial (message);
      valent_object_lock (VALENT_OBJECT (self));
      pending = g_hash_table_contains (self->pending,
                                       GUINT_TO_POINTER (reply_serial));
      valent_object_unlock (VALENT_OBJECT (self));

      if (pending)
        {
          g_autoptr (GTask) task = NULL;

          task = g_task_new (self, NULL, NULL, NULL);
          g_task_set_source_tag (task, valent_fdo_notifications_filter);
          g_task_set_task_data (task, g_object_ref (message), g_object_unref);

          g_main_context_invoke_full (NULL,
                                      g_task_get_priority (task),
                                      valent_fdo_notifications_filter_main,
                                      g_object_ref (task),
                                      g_object_unref);
        }
    }

  return message;
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
  ValentNotificationsAdapter *adapter = VALENT_NOTIFICATIONS_ADAPTER (user_data);

  g_assert (VALENT_IS_NOTIFICATIONS_ADAPTER (adapter));
  g_assert (g_strcmp0 (signal_name, "NotificationClosed") == 0);

  _notification_closed (adapter, parameters);
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

  self->name_owner = g_strdup (name_owner);
  g_set_object (&self->session, connection);

  if (self->closed_id == 0)
    {
      self->closed_id =
        g_dbus_connection_signal_subscribe (self->session,
                                            "org.freedesktop.Notifications",
                                            "org.freedesktop.Notifications",
                                            "NotificationClosed",
                                            "/org/freedesktop/Notifications",
                                            NULL,
                                            G_DBUS_SIGNAL_FLAGS_NONE,
                                            on_notification_closed,
                                            self, NULL);
    }

  if (self->filter_id == 0)
    {
      g_dbus_connection_add_filter (self->monitor,
                                    valent_fdo_notifications_filter,
                                    self,
                                    NULL);
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

  g_clear_pointer (&self->name_owner, g_free);

  if (self->closed_id > 0)
    {
      g_dbus_connection_signal_unsubscribe (self->session, self->closed_id);
      self->closed_id = 0;
    }

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
      return g_task_return_error (task, g_steal_pointer (&error));
    }

  /* Watch the true name owner */
  self->name_owner_id = g_bus_watch_name (G_BUS_TYPE_SESSION,
                                          "org.freedesktop.Notifications",
                                          G_BUS_NAME_WATCHER_FLAGS_NONE,
                                          on_name_appeared,
                                          on_name_vanished,
                                          self, NULL);


  /* Report the adapter as active */
  valent_extension_plugin_state_changed (VALENT_EXTENSION (self),
                                         VALENT_PLUGIN_STATE_ACTIVE,
                                         NULL);
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
      return g_task_return_error (task, g_steal_pointer (&error));
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
      valent_extension_plugin_state_changed (VALENT_EXTENSION (self),
                                             VALENT_PLUGIN_STATE_ERROR,
                                             error);
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

  /* Get a bus address */
  address = g_dbus_address_get_for_bus_sync (G_BUS_TYPE_SESSION,
                                             cancellable,
                                             &error);

  if (address == NULL)
    {
      valent_extension_plugin_state_changed (VALENT_EXTENSION (initable),
                                             VALENT_PLUGIN_STATE_ERROR,
                                             error);
      return g_task_return_error (task, g_steal_pointer (&error));
    }

  /* Get a dedicated connection for monitoring */
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

  if (self->closed_id > 0)
    {
      g_dbus_connection_signal_unsubscribe (self->session, self->closed_id);
      self->closed_id = 0;
    }

  if (self->filter_id > 0)
    {
      g_dbus_connection_remove_filter (self->monitor, self->filter_id);
      self->filter_id = 0;
    }

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
  g_clear_object (&self->session);

  VALENT_OBJECT_CLASS (valent_fdo_notifications_parent_class)->destroy (object);
}

/*
 * GObject
 */
static void
valent_fdo_notifications_finalize (GObject *object)
{
  ValentFdoNotifications *self = VALENT_FDO_NOTIFICATIONS (object);

  valent_object_lock (VALENT_OBJECT (self));
  g_clear_pointer (&self->node_info, g_dbus_node_info_unref);
  g_clear_pointer (&self->pending, g_hash_table_unref);
  valent_object_unlock (VALENT_OBJECT (self));

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
  self->node_info = g_dbus_node_info_new_for_xml (interface_xml, NULL);
  self->iface_info = self->node_info->interfaces[0];

  self->vtable.method_call = valent_fdo_notifications_method_call;
  self->vtable.get_property = NULL;
  self->vtable.set_property = NULL;

  valent_object_lock (VALENT_OBJECT (self));
  self->pending = g_hash_table_new_full (NULL,
                                         NULL,
                                         NULL,
                                         g_object_unref);
  valent_object_unlock (VALENT_OBJECT (self));
}

