// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-bluez-profile"

#include "config.h"

#include <gio/gio.h>
#include <gio/gunixfdlist.h>

#include "valent-bluez-profile.h"


struct _ValentBluezProfile
{
  GDBusInterfaceSkeleton  parent_instance;

  GDBusInterfaceVTable    vtable;
  GDBusNodeInfo          *node_info;
  GDBusInterfaceInfo     *iface_info;

  GVariant               *options;
  unsigned int            exported : 1;
  unsigned int            registered : 1;
};

G_DEFINE_FINAL_TYPE (ValentBluezProfile, valent_bluez_profile, G_TYPE_DBUS_INTERFACE_SKELETON)

enum {
  CONNECTION_OPENED,
  CONNECTION_CLOSED,
  N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0, };


/*
 * org.bluez.Profile1
 */
static const char interface_xml[] =
  "<node>"
  "  <interface name='org.bluez.Profile1'>"
  "    <method name='Release'/>"
  "    <method name='NewConnection'>"
  "     <arg name='device' type='o' direction='in'/>"
  "     <arg name='fd' type='h' direction='in'/>"
  "     <arg name='fd_properties' type='a{sv}' direction='in'/>"
  "   </method>"
  "   <method name='RequestDisconnection'>"
  "     <arg name='object_path' type='o' direction='in'/>"
  "   </method>"
  "  </interface>"
  "</node>";


/**
 * valent_bluez_profile_new_connection:
 * @profile: a #ValentBluezProfile
 * @object_path: a DBus object path
 * @fd: a UNIX file descriptor
 * @fd_properties: a #GVariant
 *
 * This method gets called when a new service level connection has been made and
 * authorized.
 */
static void
valent_bluez_profile_new_connection (ValentBluezProfile *profile,
                                     const char         *object_path,
                                     int                 fd,
                                     GVariant           *fd_properties)
{
  g_autoptr (GError) error = NULL;
  g_autoptr (GSocket) socket = NULL;
  g_autoptr (GSocketConnection) connection = NULL;

  g_assert (VALENT_IS_BLUEZ_PROFILE (profile));
  g_assert (g_variant_is_object_path (object_path));

  if ((socket = g_socket_new_from_fd (fd, &error)) == NULL)
    {
      g_warning ("Failed to create socket: %s", error->message);
      return;
    }

  connection = g_object_new (G_TYPE_SOCKET_CONNECTION,
                             "socket", socket,
                             NULL);

  g_signal_emit (G_OBJECT (profile),
                 signals [CONNECTION_OPENED], 0,
                 connection,
                 object_path);
}

/**
 * valent_bluez_profile_request_disconnection:
 * @profile: a #ValentBluezProfile
 * @object_path: a DBus object path
 *
 * This method gets called when a profile gets disconnected.
 *
 * The file descriptor is no longer owned by the service daemon and the profile
 * implementation needs to take care of cleaning up all connections.
 *
 * If multiple file descriptors are indicated via
 * valent_bluez_profile_new_connection(), it is expected that all of them are
 * disconnected before returning from this method call.
 */
static void
valent_bluez_profile_request_disconnection (ValentBluezProfile *profile,
                                            const char         *object_path)
{
  g_assert (VALENT_IS_BLUEZ_PROFILE (profile));

  g_signal_emit (G_OBJECT (profile),
                 signals [CONNECTION_CLOSED], 0,
                 object_path);
}

/**
 * valent_bluez_profile_release:
 * @profile: a #ValentBluezProfile
 *
 * This method gets called when the service daemon unregisters the profile.
 * A profile can use it to do cleanup tasks. There is no need to unregister
 * the profile, because when this method gets called it has already been
 * unregistered.
 */
static void
valent_bluez_profile_release (ValentBluezProfile *profile)
{
  g_assert (VALENT_IS_BLUEZ_PROFILE (profile));

  profile->registered = FALSE;
  valent_bluez_profile_unregister (profile);
}


/*
 * GDBusInterfaceSkeleton
 */
static GDBusInterfaceInfo *
valent_bluez_profile_get_info (GDBusInterfaceSkeleton *skeleton)
{
  ValentBluezProfile *self = VALENT_BLUEZ_PROFILE (skeleton);

  return self->iface_info;
}

static GDBusInterfaceVTable *
valent_bluez_profile_get_vtable (GDBusInterfaceSkeleton *skeleton)
{
  ValentBluezProfile *self = VALENT_BLUEZ_PROFILE (skeleton);

  return &(self->vtable);
}

static void
valent_bluez_profile_method_call (GDBusConnection       *connection,
                                  const char            *sender,
                                  const char            *object_path,
                                  const char            *interface_name,
                                  const char            *method_name,
                                  GVariant              *parameters,
                                  GDBusMethodInvocation *invocation,
                                  gpointer               user_data)
{
  ValentBluezProfile *profile = VALENT_BLUEZ_PROFILE (user_data);

  g_assert (VALENT_IS_BLUEZ_PROFILE (profile));

  if (g_strcmp0 (method_name, "NewConnection") == 0)
    {
      const char *device;
      gint32 fd_idx;
      GDBusMessage *message;
      GUnixFDList *fd_list;
      int fd;
      g_autoptr (GVariant) fd_props = NULL;

      g_assert (g_variant_is_of_type (parameters, G_VARIANT_TYPE ("(oha{sv})")));

      g_variant_get (parameters, "(&oh@a{sv})", &device, &fd_idx, &fd_props);

      /* Get the file descriptor */
      message = g_dbus_method_invocation_get_message (invocation);
      fd_list = g_dbus_message_get_unix_fd_list (message);
      fd = g_unix_fd_list_get (fd_list, fd_idx, NULL);

      valent_bluez_profile_new_connection (profile, device, fd, fd_props);
    }
  else if (g_strcmp0 (method_name, "RequestDisconnection") == 0)
    {
      const char *device;

      g_assert (g_variant_is_of_type (parameters, G_VARIANT_TYPE ("(o)")));

      g_variant_get (parameters, "(&o)", &device);
      valent_bluez_profile_request_disconnection (profile, device);
    }
  else if (g_strcmp0 (method_name, "Release") == 0)
    {
      g_assert (g_variant_is_of_type (parameters, G_VARIANT_TYPE ("()")));

      valent_bluez_profile_release (profile);
    }
  else
    g_assert_not_reached();

  g_dbus_method_invocation_return_value (invocation, NULL);
}

static GVariant *
valent_bluez_profile_get_properties (GDBusInterfaceSkeleton *skeleton) {
  return NULL;
}

static void
valent_bluez_profile_flush (GDBusInterfaceSkeleton *skeleton) {
  return;
}

/*
 * GObject
 */
static void
valent_bluez_profile_dispose (GObject *object)
{
  ValentBluezProfile *self = VALENT_BLUEZ_PROFILE (object);

  valent_bluez_profile_unregister (self);

  G_OBJECT_CLASS (valent_bluez_profile_parent_class)->dispose (object);
}

static void
valent_bluez_profile_finalize (GObject *object)
{
  ValentBluezProfile *self = VALENT_BLUEZ_PROFILE (object);

  g_clear_pointer (&self->node_info, g_dbus_node_info_unref);
  g_clear_pointer (&self->options, g_variant_unref);

  G_OBJECT_CLASS (valent_bluez_profile_parent_class)->finalize (object);
}

static void
valent_bluez_profile_class_init (ValentBluezProfileClass *klass) {
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GDBusInterfaceSkeletonClass *skeleton_class = G_DBUS_INTERFACE_SKELETON_CLASS (klass);

  object_class->dispose = valent_bluez_profile_dispose;
  object_class->finalize = valent_bluez_profile_finalize;

  skeleton_class->get_info = valent_bluez_profile_get_info;
  skeleton_class->get_vtable = valent_bluez_profile_get_vtable;
  skeleton_class->get_properties = valent_bluez_profile_get_properties;
  skeleton_class->flush = valent_bluez_profile_flush;

  /**
   * ValentBluezProfile::connection-opened:
   * @profile: a #ValentBluezProfile
   * @connection: a #GSocketConnection
   * @device: DBus object path of the device
   *
   * The #ValentBluezProfile::connection-opened signal is emitted when a Bluez
   * socket for @device has been successfully wrapped in a #GSocketConnection
   * and is ready for protocol negotiation.
   */
  signals [CONNECTION_OPENED] =
    g_signal_new ("connection-opened",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 2, G_TYPE_SOCKET_CONNECTION, G_TYPE_STRING);

  /**
   * ValentBluezProfile::connection-closed:
   * @profile: a #ValentBluezProfile
   * @device: DBus object path of the device
   *
   * The #ValentBluezProfile::connection-closed signal is emitted when a Bluez
   * socket for @device has been closed and should be cleaned up.
   */
  signals [CONNECTION_CLOSED] =
    g_signal_new ("connection-closed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__STRING,
                  G_TYPE_NONE, 1, G_TYPE_STRING);
  g_signal_set_va_marshaller (signals [CONNECTION_CLOSED],
                              G_TYPE_FROM_CLASS (klass),
                              g_cclosure_marshal_VOID__STRINGv);
}

static void
valent_bluez_profile_init (ValentBluezProfile *self)
{
  g_autoptr (GBytes) bytes = NULL;
  const char *sdp_xml;
  GVariantDict dict;

  self->node_info = g_dbus_node_info_new_for_xml (interface_xml, NULL);
  self->iface_info = g_dbus_node_info_lookup_interface (self->node_info,
                                                        "org.bluez.Profile1");

  self->vtable.method_call = valent_bluez_profile_method_call;
  self->vtable.get_property = NULL;
  self->vtable.set_property = NULL;

  /* Lookup the SDP Record */
  bytes = g_resources_lookup_data ("/plugins/bluez/ca.andyholmes.Valent.sdp.xml",
                                   G_RESOURCE_LOOKUP_FLAGS_NONE,
                                   NULL);
  sdp_xml = g_bytes_get_data (bytes, NULL);

  /* Create a reffed options variant */
  g_variant_dict_init (&dict, NULL);
  g_variant_dict_insert (&dict, "RequireAuthorization", "b", FALSE);
  g_variant_dict_insert (&dict, "RequireAuthentication", "b", FALSE);
  g_variant_dict_insert (&dict, "ServiceRecord", "s", sdp_xml);
  self->options = g_variant_ref_sink (g_variant_dict_end (&dict));
}

static void
profile_manager_register_profile_cb (GDBusConnection *connection,
                                     GAsyncResult    *result,
                                     gpointer         user_data)
{
  g_autoptr (GTask) task = G_TASK (user_data);
  ValentBluezProfile *self = g_task_get_source_object (task);
  g_autoptr (GVariant) reply = NULL;
  GError *error = NULL;

  reply = g_dbus_connection_call_finish (connection, result, &error);
  self->registered = (error == NULL);

  if (!self->registered)
    return g_task_return_error (task, error);

  g_task_return_boolean (task, TRUE);
}

/**
 * valent_bluez_profile_register:
 * @profile: a #ValentBluezProfile
 * @connection: a #GDBusConnection
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): a #GAsyncReadyCallback
 * @user_data: (closure): user supplied data
 *
 * Export the bluez profile for Valent on @connection and register it with the
 * profile manager (`org.bluez.ProfileManager1`).
 */
void
valent_bluez_profile_register (ValentBluezProfile  *profile,
                               GDBusConnection     *connection,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
  GDBusInterfaceSkeleton *iface = G_DBUS_INTERFACE_SKELETON (profile);
  g_autoptr (GTask) task = NULL;
  GError *error = NULL;

  g_return_if_fail (VALENT_IS_BLUEZ_PROFILE (profile));
  g_return_if_fail (G_IS_DBUS_CONNECTION (connection));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (profile, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_bluez_profile_register);

  if (profile->registered)
    return g_task_return_boolean (task, TRUE);

  /* Export the org.bluez.Profile1 Interface */
  if (!profile->exported)
    {
      profile->exported = g_dbus_interface_skeleton_export (iface,
                                                            connection,
                                                            VALENT_BLUEZ_PROFILE_PATH,
                                                            &error);

      if (!profile->exported)
        return g_task_return_error (task, error);
    }

  /* Register the profile with bluez */
  profile->registered = TRUE;
  g_dbus_connection_call (connection,
                          "org.bluez",
                          "/org/bluez",
                          "org.bluez.ProfileManager1",
                          "RegisterProfile",
                          g_variant_new ("(os@a{sv})",
                                         VALENT_BLUEZ_PROFILE_PATH,
                                         VALENT_BLUEZ_PROFILE_UUID,
                                         profile->options),
                          NULL,
                          G_DBUS_CALL_FLAGS_NO_AUTO_START,
                          -1,
                          cancellable,
                          (GAsyncReadyCallback)profile_manager_register_profile_cb,
                          g_steal_pointer (&task));
}

/**
 * valent_bluez_profile_register_finish:
 * @profile: a #ValentBluezProfile
 * @result: a #GAsyncResult
 * @error: (nullable): a #GError
 *
 * Finish an operation started with valent_bluez_profile_register().
 *
 * Returns: %TRUE if successful, or %FALSE with @error set
 */
gboolean
valent_bluez_profile_register_finish (ValentBluezProfile  *profile,
                                      GAsyncResult        *result,
                                      GError             **error)
{
  g_return_val_if_fail (VALENT_IS_BLUEZ_PROFILE (profile), FALSE);
  g_return_val_if_fail (g_task_is_valid (result, profile), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}


/**
 * valent_bluez_profile_unregister:
 * @profile: a #ValentBluezProfile
 *
 * Unexport the bluez profile for Valent from the system bus.
 */
void
valent_bluez_profile_unregister (ValentBluezProfile *profile)
{
  GDBusInterfaceSkeleton *iface = G_DBUS_INTERFACE_SKELETON (profile);

  g_return_if_fail (VALENT_IS_BLUEZ_PROFILE (profile));

  if (profile->registered)
    {
      profile->registered = FALSE;
      g_dbus_connection_call (g_dbus_interface_skeleton_get_connection (iface),
                              "org.bluez",
                              "/org/bluez",
                              "org.bluez.ProfileManager1",
                              "UnregisterProfile",
                              g_variant_new ("(o)", VALENT_BLUEZ_PROFILE_PATH),
                              NULL,
                              G_DBUS_CALL_FLAGS_NO_AUTO_START,
                              -1,
                              NULL,
                              NULL,
                              NULL);
    }

  if (profile->exported)
    {
      profile->exported = FALSE;
      g_dbus_interface_skeleton_unexport (G_DBUS_INTERFACE_SKELETON (profile));
    }
}

