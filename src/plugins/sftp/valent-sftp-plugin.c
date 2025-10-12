// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-sftp-plugin"

#include "config.h"

#include <glib/gi18n.h>
#include <gio/gio.h>
#include <json-glib/json-glib.h>
#include <valent.h>

#include "valent-sftp-plugin.h"


typedef struct _ValentSftpSession ValentSftpSession;

struct _ValentSftpPlugin
{
  ValentDevicePlugin  parent_instance;

  GDBusConnection    *connection;
  GVolumeMonitor     *monitor;
  ValentSftpSession  *session;
  unsigned int        privkey_ready : 1;
};

static void   valent_sftp_plugin_mount_volume   (ValentSftpPlugin *self);
static void   valent_sftp_plugin_unmount_volume (ValentSftpPlugin *self);
static void   valent_sftp_plugin_update_menu    (ValentSftpPlugin *self);

G_DEFINE_FINAL_TYPE (ValentSftpPlugin, valent_sftp_plugin, VALENT_TYPE_DEVICE_PLUGIN)

/*< private >
 * get_device_host:
 * @self: a `ValentSftpPlugin`
 *
 * Try to find a IP-based device channel with a host.
 *
 * Returns: (nullable) (transfer full): a hostname or IP address
 */
static char *
get_device_host (ValentSftpPlugin *self)
{
  ValentDevice *device;
  char *host = NULL;

  g_assert (VALENT_IS_SFTP_PLUGIN (self));

  device = valent_resource_get_source (VALENT_RESOURCE (self));
  if (device != NULL)
    {
      GListModel *channels;
      unsigned int n_channels;

      channels = valent_device_get_channels (device);
      n_channels = g_list_model_get_n_items (channels);
      for (unsigned int i = 0; i < n_channels; i++)
        {
          g_autoptr (ValentChannel) channel = NULL;
          GParamSpec *pspec = NULL;

          channel = g_list_model_get_item (channels, i);
          pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (channel),
                                                "host");
          if (pspec != NULL)
            {
              g_object_get (channel, "host", &host, NULL);
              break;
            }
        }
    }

  return g_steal_pointer (&host);
}

/**
 * ValentSftpSession:
 * @host: Host name or IP
 * @port: Port
 * @username: Username (deprecated)
 * @password: Password (deprecated)
 * @mount: A `GMount` for the session
 *
 * `ValentSftpSession` is a simple representation of a SFTP session.
 */
typedef struct _ValentSftpSession
{
  char       *host;
  uint16_t    port;
  char       *username;
  char       *password;
  GHashTable *paths;

  /* Gvfs state */
  GMount     *mount;
  char       *uri;
} ValentSftpSession;


static ValentSftpSession *
sftp_session_new (ValentSftpPlugin *self,
                  JsonNode         *packet)
{
  ValentSftpSession *session;
  g_autofree char *host = NULL;
  int64_t port;
  const char *password;
  const char *username;
  JsonArray *multi_paths = NULL;
  JsonArray *path_names = NULL;

  /* Ultimately, these are the only packet fields we really need */
  if (!valent_packet_get_int (packet, "port", &port) ||
      (port < 0 || port > G_MAXUINT16))
    {
      g_debug ("%s(): expected \"port\" field holding a uint16",
               G_STRFUNC);
      return NULL;
    }

  if ((host = get_device_host (self)) == NULL)
    {
      g_warning ("%s(): failed to get host address",
                 G_STRFUNC);
      return NULL;
    }

  // Create a session struct
  session = g_new0 (ValentSftpSession, 1);
  session->host = g_steal_pointer (&host);
  session->port = (uint16_t)port;
  session->paths = g_hash_table_new_full (g_str_hash, g_str_equal,
                                          g_free, g_free);
  session->uri = g_strdup_printf ("sftp://%s:%u/", session->host, session->port);

  if (valent_packet_get_string (packet, "user", &username))
    session->username = g_strdup (username);

  if (valent_packet_get_string (packet, "password", &password))
    session->password = g_strdup (password);

  if (valent_packet_get_array (packet, "multiPaths", &multi_paths) &&
      valent_packet_get_array (packet, "pathNames", &path_names))
    {
      unsigned int n_paths = json_array_get_length (multi_paths);
      unsigned int n_names = json_array_get_length (path_names);

      for (unsigned int i = 0; i < n_paths && i < n_names; i++)
        {
          const char *path = json_array_get_string_element (multi_paths, i);
          const char *name = json_array_get_string_element (path_names, i);
          g_autofree char *uri = NULL;

          uri = g_strdup_printf ("sftp://%s:%u%s",
                                 session->host,
                                 session->port,
                                 path);
          g_hash_table_replace (session->paths,
                                g_steal_pointer (&uri),
                                g_strdup (name));
        }
    }

  return session;
}

static void
sftp_session_free (gpointer data)
{
  ValentSftpSession *session = data;

  g_clear_pointer (&session->host, g_free);
  g_clear_pointer (&session->username, g_free);
  g_clear_pointer (&session->password, g_free);
  g_clear_pointer (&session->paths, g_hash_table_unref);

  g_clear_object (&session->mount);
  g_clear_pointer (&session->uri, g_free);

  g_free (session);
}

static void
sftp_session_end_cb (GMount       *mount,
                     GAsyncResult *result,
                     gpointer      user_data)
{
  g_autoptr (GError) error = NULL;

  if (!g_mount_unmount_with_operation_finish (mount, result, &error))
    g_debug ("Failed unmounting: %s", error->message);
}

static void
sftp_session_end (gpointer data)
{
  ValentSftpSession *session = data;
  g_autoptr (GMount) mount = NULL;
  g_autoptr (GMountOperation) op = NULL;

  mount = g_steal_pointer (&session->mount);
  sftp_session_free (session);

  if (mount == NULL)
    return;

  op = g_mount_operation_new ();
  g_mount_unmount_with_operation (mount,
                                  G_MOUNT_UNMOUNT_FORCE,
                                  op,
                                  NULL,
                                  (GAsyncReadyCallback)sftp_session_end_cb,
                                  NULL);
}

static gboolean
sftp_session_find (ValentSftpPlugin *self)
{
  g_autofree char *host = NULL;
  g_autofree char *host_pattern = NULL;
  g_autoptr (GRegex) regex = NULL;
  g_autolist (GMount) mounts = NULL;

  if (self->session && self->session->mount)
    return TRUE;

  if ((host = get_device_host (self)) == NULL)
    return FALSE;

  // TODO: is this reasonable?
  host_pattern = g_strdup_printf ("sftp://(%s):([22-65535])", host);
  regex = g_regex_new (host_pattern, G_REGEX_OPTIMIZE, 0, NULL);

  /* Search through each mount in the volume monitor... */
  mounts = g_volume_monitor_get_mounts (self->monitor);

  for (const GList *iter = mounts; iter; iter = iter->next)
    {
      g_autoptr (GFile) root = NULL;
      g_autofree char *uri = NULL;

      root = g_mount_get_root (iter->data);
      uri = g_file_get_uri (root);

      /* The URI matches our mount */
      if (g_regex_match (regex, uri, 0, NULL))
        {
          if (self->session == NULL)
            self->session = g_new0 (ValentSftpSession, 1);

          g_set_object (&self->session->mount, iter->data);
          g_set_str (&self->session->uri, uri);

          valent_sftp_plugin_update_menu (self);
          return TRUE;
        }
    }

  return FALSE;
}


/*
 * GVolumeMonitor Callbacks
 */
static void
on_mount_added (GVolumeMonitor   *volume_monitor,
                GMount           *mount,
                ValentSftpPlugin *self)
{
  g_autoptr (GFile) root = NULL;
  g_autofree char *uri = NULL;

  g_assert (VALENT_IS_SFTP_PLUGIN (self));

  if (self->session == NULL)
    return;

  root = g_mount_get_root (mount);
  uri = g_file_get_uri (root);

  if (g_strcmp0 (self->session->uri, uri) == 0)
    {
      g_set_object (&self->session->mount, mount);
      valent_sftp_plugin_update_menu (self);
    }
}

static void
on_mount_removed (GVolumeMonitor   *volume_monitor,
                  GMount           *mount,
                  ValentSftpPlugin *self)
{
  g_autoptr (GFile) root = NULL;
  g_autofree char *uri = NULL;

  g_assert (VALENT_IS_SFTP_PLUGIN (self));

  if (self->session == NULL)
    return;

  root = g_mount_get_root (mount);
  uri = g_file_get_uri (root);

  if (g_strcmp0 (self->session->uri, uri) == 0)
    {
      g_clear_object (&self->session->mount);
      valent_sftp_plugin_update_menu (self);
    }
}

/**
 * remove_host_key:
 * @host: An IP or hostname
 *
 * Remove all host keys associated with @host from the ssh-agent.
 */
static void
remove_host_key (const char *host)
{
  g_assert (host != NULL);

  for (uint16_t port = 1739; port <= 1764; port++)
    {
      g_autoptr (GSubprocess) proc = NULL;
      g_autofree char *match = NULL;

      match = g_strdup_printf ("[%s]:%d", host, port);
      proc = g_subprocess_new (G_SUBPROCESS_FLAGS_STDOUT_SILENCE |
                               G_SUBPROCESS_FLAGS_STDERR_MERGE,
                               NULL,
                               "ssh-keygen", "-R", match,
                               NULL);

      if (proc != NULL)
        g_subprocess_wait_async (proc, NULL, NULL, NULL);
    }
}

/*
 * GMountOperation
 */
static void
ask_password_cb (GMountOperation   *operation,
                 char              *message,
                 char              *default_user,
                 char              *default_domain,
                 GAskPasswordFlags  flags,
                 ValentSftpPlugin  *self)
{
  g_assert (VALENT_IS_SFTP_PLUGIN (self));
  g_return_if_fail (self->session != NULL);

  /* The username/password are only set in response to a request, to prefer
   * public key authentication when possible.
   */
  if ((flags & G_ASK_PASSWORD_NEED_USERNAME) != 0)
    g_mount_operation_set_username (operation, self->session->username);

  if ((flags & G_ASK_PASSWORD_NEED_PASSWORD) != 0)
    {
      g_mount_operation_set_password (operation, self->session->password);
      g_mount_operation_set_password_save (operation, G_PASSWORD_SAVE_NEVER);
    }

  g_mount_operation_reply (operation, G_MOUNT_OPERATION_HANDLED);
}

static void
ask_question_cb (GMountOperation *operation,
                 char            *message,
                 GStrv            choices,
                 gpointer         user_data)
{
  /* Host keys are automatically accepted, since we use the host address of
   * the authenticated device connection.
   */
  g_mount_operation_reply (operation, G_MOUNT_OPERATION_HANDLED);
}

static void
g_file_mount_enclosing_volume_cb (GFile        *file,
                                  GAsyncResult *result,
                                  gpointer      user_data)
{
  g_autoptr (ValentSftpPlugin) self = VALENT_SFTP_PLUGIN (g_steal_pointer (&user_data));
  g_autoptr (GError) error = NULL;

  g_assert (VALENT_IS_SFTP_PLUGIN (self));
  g_return_if_fail (self->session != NULL);

  /* On success we will acquire the mount from the volume monitor */
  if (g_file_mount_enclosing_volume_finish (file, result, &error))
    return;

  /* On the off-chance this happens, we will just ensure we have the mount */
  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_ALREADY_MOUNTED) &&
      sftp_session_find (self))
    return;

  /* On failure, we're particularly interested in host key failures so that
   * we can remove those from the ssh-agent. These are reported by GVfs as
   * G_IO_ERROR_FAILED with a localized string, so we just assume. */
  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_FAILED))
    {
      g_warning ("%s(): Error mounting: %s", G_STRFUNC, error->message);

      if (self->session && self->session->host)
        remove_host_key (self->session->host);
    }

  g_clear_pointer (&self->session, sftp_session_free);
}

static void
valent_sftp_plugin_mount_volume (ValentSftpPlugin *self)
{
  g_autoptr (GFile) file = NULL;
  g_autoptr (GMountOperation) operation = NULL;
  g_autoptr (GCancellable) destroy = NULL;

  g_assert (VALENT_IS_SFTP_PLUGIN (self));
  g_return_if_fail (self->session != NULL);

  file = g_file_new_for_uri (self->session->uri);
  operation = g_mount_operation_new ();
  g_signal_connect_object (operation,
                           "ask-password",
                           G_CALLBACK (ask_password_cb),
                           self,
                           G_CONNECT_DEFAULT);
  g_signal_connect_object (operation,
                           "ask-question",
                           G_CALLBACK (ask_question_cb),
                           self,
                           G_CONNECT_DEFAULT);

  destroy = valent_object_ref_cancellable (VALENT_OBJECT (self));
  g_file_mount_enclosing_volume (file,
                                 G_MOUNT_MOUNT_NONE,
                                 operation,
                                 destroy,
                                 (GAsyncReadyCallback)g_file_mount_enclosing_volume_cb,
                                 g_object_ref (self));

}

static void
g_mount_unmount_with_operation_cb (GMount       *mount,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  g_autoptr (GError) error = NULL;

  if (!g_mount_unmount_with_operation_finish (mount, result, &error))
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s(): Error unmounting: %s", G_STRFUNC, error->message);

      return;
    }
}

static void
valent_sftp_plugin_unmount_volume (ValentSftpPlugin *self)
{
  g_autoptr (GMountOperation) operation = NULL;
  g_autoptr (GCancellable) destroy = NULL;

  g_assert (VALENT_IS_SFTP_PLUGIN (self));
  g_return_if_fail (self->session != NULL);

  if (self->session->mount == NULL)
    return;

  operation = g_mount_operation_new ();
  destroy = valent_object_ref_cancellable (VALENT_OBJECT (self));
  g_mount_unmount_with_operation (self->session->mount,
                                  G_MOUNT_UNMOUNT_NONE,
                                  operation,
                                  destroy,
                                  (GAsyncReadyCallback)g_mount_unmount_with_operation_cb,
                                  NULL);
}

static void
valent_sftp_plugin_register_privkey_cb (GSubprocess  *proc,
                                        GAsyncResult *result,
                                        gpointer      user_data)
{
  g_autoptr (ValentSftpPlugin) self = VALENT_SFTP_PLUGIN (g_steal_pointer (&user_data));
  g_autoptr (GError) error = NULL;

  if (!g_subprocess_wait_check_finish (proc, result, &error))
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          g_warning ("%s(): Failed to register private key: %s",
                     G_STRFUNC,
                     error->message);
        }

      g_clear_pointer (&self->session, sftp_session_free);
      return;
    }

  self->privkey_ready = TRUE;
  valent_sftp_plugin_mount_volume (self);
}

static void
valent_sftp_plugin_register_privkey (ValentSftpPlugin  *self)
{
  g_autoptr (ValentContext) context = NULL;
  g_autoptr (GSubprocess) proc = NULL;
  g_autoptr (GFile) private_key = NULL;
  g_autoptr (GCancellable) destroy = NULL;
  g_autoptr (GError) error = NULL;

  g_assert (VALENT_IS_SFTP_PLUGIN (self));
  g_return_if_fail (self->session != NULL);

  /* Get the root context and add the private key to the ssh-agent */
  context = valent_context_new (NULL, NULL, NULL);
  private_key = valent_context_get_config_file (context, "private.pem");
  proc = g_subprocess_new (G_SUBPROCESS_FLAGS_STDOUT_SILENCE |
                           G_SUBPROCESS_FLAGS_STDERR_MERGE,
                           &error,
                           "ssh-add", g_file_peek_path (private_key),
                           NULL);

  if (proc == NULL)
    {
      g_warning ("%s(): Failed to add host key: %s", G_STRFUNC, error->message);
      g_clear_pointer (&self->session, sftp_session_free);
      return;
    }

  destroy = valent_object_ref_cancellable (VALENT_OBJECT (self));
  g_subprocess_wait_check_async (proc,
                                 destroy,
                                 (GAsyncReadyCallback)valent_sftp_plugin_register_privkey_cb,
                                 g_object_ref (self));
}

/*
 * Packet Handlers
 */
static void
handle_sftp_error (ValentSftpPlugin *self,
                   JsonNode         *packet)
{
  ValentDevice *device;
  g_autoptr (GNotification) notification = NULL;
  g_autoptr (GIcon) error_icon = NULL;
  g_autofree char *error_title = NULL;
  const char *error_message;
  const char *device_name;
  JsonObject *body;

  g_assert (VALENT_IS_SFTP_PLUGIN (self));

  body = valent_packet_get_body (packet);

  device = valent_resource_get_source (VALENT_RESOURCE (self));
  device_name = valent_device_get_name (device);

  error_icon = g_themed_icon_new ("dialog-error-symbolic");
  error_title = g_strdup_printf ("%s: SFTP", device_name);
  error_message = json_object_get_string_member (body, "errorMessage");

  /* Send notification */
  notification = g_notification_new (error_title);
  g_notification_set_body (notification, error_message);
  g_notification_set_icon (notification, error_icon);
  g_notification_set_priority (notification, G_NOTIFICATION_PRIORITY_HIGH);
  valent_device_plugin_show_notification (VALENT_DEVICE_PLUGIN (self),
                                          "sftp-error",
                                          notification);
}

static void
handle_sftp_mount (ValentSftpPlugin *self,
                   JsonNode         *packet)
{
  g_assert (VALENT_IS_SFTP_PLUGIN (self));

  /* Check if we're already mounted or mounting */
  if (self->session != NULL)
    return;

  /* Parse the connection data */
  self->session = sftp_session_new (self, packet);
  if (self->session != NULL)
    {
      if (self->privkey_ready)
        valent_sftp_plugin_mount_volume (self);
      else
        valent_sftp_plugin_register_privkey (self);
    }
}

static void
valent_sftp_plugin_handle_sftp (ValentSftpPlugin *self,
                                JsonNode         *packet)
{
  g_assert (VALENT_IS_SFTP_PLUGIN (self));

  /* The request for mount information failed, most likely due to the remote
   * device not being setup yet.
   */
  if (valent_packet_check_field (packet, "errorMessage"))
    handle_sftp_error (self, packet);

  /* Otherwise we've been sent the information necessary to open an SSH/SFTP
   * connection to the remote device.
   */
  else
    handle_sftp_mount (self, packet);
}

/*
 * Packet Providers
 */
static void
valent_sftp_plugin_handle_request (ValentSftpPlugin *self,
                                   JsonNode         *packet)
{
  GSettings *settings;
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonNode) response = NULL;

  g_assert (VALENT_IS_SFTP_PLUGIN (self));

  if (!valent_packet_check_field (packet, "startBrowsing"))
    return;

  settings = valent_extension_get_settings (VALENT_EXTENSION (self));
  valent_packet_init (&builder, "kdeconnect.sftp");

  if (g_settings_get_boolean (settings, "local-allow"))
    {
      uint16_t local_port;

      json_builder_set_member_name (builder, "user");
      json_builder_add_string_value (builder, g_get_user_name ());

      local_port = g_settings_get_uint (settings, "local-port");
      json_builder_set_member_name (builder, "port");
      json_builder_add_int_value (builder, local_port);

      json_builder_set_member_name (builder, "multiPaths");
      json_builder_begin_array (builder);
      json_builder_add_string_value (builder, g_get_home_dir ());
      json_builder_end_array (builder);

      json_builder_set_member_name (builder, "pathNames");
      json_builder_begin_array (builder);
      json_builder_add_string_value (builder, _("Home"));
      json_builder_end_array (builder);
    }
  else
    {
      json_builder_set_member_name (builder, "errorMessage");
      json_builder_add_string_value (builder, _("Permission denied"));
    }

  response = valent_packet_end (&builder);
  valent_device_plugin_queue_packet (VALENT_DEVICE_PLUGIN (self), response);
}

static void
valent_sftp_plugin_sftp_request (ValentSftpPlugin *self)
{
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonNode) packet = NULL;

  g_assert (VALENT_IS_SFTP_PLUGIN (self));

  if (sftp_session_find (self))
    return;

  valent_packet_init (&builder, "kdeconnect.sftp.request");
  json_builder_set_member_name (builder, "startBrowsing");
  json_builder_add_boolean_value (builder, TRUE);
  packet = valent_packet_end (&builder);

  valent_device_plugin_queue_packet (VALENT_DEVICE_PLUGIN (self), packet);
}

/*
 * GActions
 */
static void
valent_sftp_plugin_open_uri (ValentSftpPlugin *self,
                             const char       *uri)
{
  g_assert (VALENT_IS_SFTP_PLUGIN (self));
  g_assert (uri != NULL);

  if (self->connection != NULL)
    {
      g_dbus_connection_call (self->connection,
                              "org.freedesktop.FileManager1",
                              "/org/freedesktop/FileManager1",
                              "org.freedesktop.FileManager1",
                              "ShowFolders",
                              g_variant_new_parsed ("([%s], '')", uri),
                              NULL,
                              G_DBUS_CALL_FLAGS_NONE,
                              -1,
                              NULL, NULL, NULL);
    }
  else
    {
      g_app_info_launch_default_for_uri_async (uri, NULL, NULL, NULL, NULL);
    }
}

static void
mount_action (GSimpleAction *action,
              GVariant      *parameter,
              gpointer       user_data)
{
  ValentSftpPlugin *self = VALENT_SFTP_PLUGIN (user_data);

  g_assert (VALENT_IS_SFTP_PLUGIN (self));

  if (self->session == NULL)
    valent_sftp_plugin_sftp_request (self);
  else if (self->session->mount == NULL)
    valent_sftp_plugin_mount_volume (self);
}

static void
unmount_action (GSimpleAction *action,
                GVariant      *parameter,
                gpointer       user_data)
{
  ValentSftpPlugin *self = VALENT_SFTP_PLUGIN (user_data);

  g_assert (VALENT_IS_SFTP_PLUGIN (self));

  if (self->session != NULL && self->session->mount != NULL)
    valent_sftp_plugin_unmount_volume (self);
}

static void
open_uri_action (GSimpleAction *action,
                 GVariant      *parameter,
                 gpointer       user_data)
{
  ValentSftpPlugin *self = VALENT_SFTP_PLUGIN (user_data);
  const char *uri = NULL;

  g_assert (VALENT_IS_SFTP_PLUGIN (self));

  uri = g_variant_get_string (parameter, NULL);
  valent_sftp_plugin_open_uri (self, uri);
}

static const GActionEntry actions[] = {
    {"browse",   mount_action,    NULL, NULL, NULL},
    {"unmount",  unmount_action,  NULL, NULL, NULL},
    {"open-uri", open_uri_action, "s",  NULL, NULL},
};

/*
 * ValentSftpPlugin
 */
static void
valent_sftp_plugin_update_menu (ValentSftpPlugin *self)
{
  ValentDevicePlugin *plugin = VALENT_DEVICE_PLUGIN (self);

  g_assert (VALENT_IS_SFTP_PLUGIN (self));

  if (sftp_session_find (self))
    {
      GHashTableIter iter;
      const char *uri = NULL;
      const char *name = NULL;
      g_autoptr (GMenuItem) item = NULL;
      g_autoptr (GIcon) icon = NULL;
      g_autoptr (GMenu) submenu = NULL;
      g_autoptr (GMenuItem) unmount_item = NULL;
      g_autoptr (GIcon) unmount_icon = NULL;

      icon = g_themed_icon_new ("folder-remote-symbolic");
      item = g_menu_item_new (_("Browse Folders"), "device.sftp.browse");
      g_menu_item_set_icon (item, icon);
      g_menu_item_set_attribute (item, "hidden-when", "s", "action-disabled");

      submenu = g_menu_new ();
      g_menu_item_set_submenu (item, G_MENU_MODEL (submenu));

      g_hash_table_iter_init (&iter, self->session->paths);
      while (g_hash_table_iter_next (&iter, (void **)&uri, (void **)&name))
        {
          g_autofree char *action_name = NULL;

          action_name = g_strdup_printf ("device.sftp.open-uri::%s", uri);
          g_menu_append (submenu, name, action_name);
        }

      unmount_icon = g_themed_icon_new ("media-eject-symbolic");
      unmount_item = g_menu_item_new (_("Unmount"), "device.sftp.unmount");
      g_menu_item_set_icon (unmount_item, unmount_icon);
      g_menu_append_item (submenu, unmount_item);

      valent_device_plugin_set_menu_item (plugin, "device.sftp.browse", item);
    }
  else
    {
      valent_device_plugin_set_menu_action (plugin,
                                            "device.sftp.browse",
                                            _("Mount"),
                                            "folder-remote-symbolic");
    }
}

/*
 * ValentDevicePlugin
 */
static void
valent_sftp_plugin_update_state (ValentDevicePlugin *plugin,
                                 ValentDeviceState   state)
{
  ValentSftpPlugin *self = VALENT_SFTP_PLUGIN (plugin);
  gboolean available;

  g_assert (VALENT_IS_SFTP_PLUGIN (self));

  available = (state & VALENT_DEVICE_STATE_CONNECTED) != 0 &&
              (state & VALENT_DEVICE_STATE_PAIRED) != 0;

  valent_extension_toggle_actions (VALENT_EXTENSION (plugin), available);

  // FIXME: device may be in destruction when `get_device_host()` is invoked
  valent_sftp_plugin_update_menu (self);

  if (available)
    {
      GSettings *settings;

      settings = valent_extension_get_settings (VALENT_EXTENSION (plugin));
      if (g_settings_get_boolean (settings, "auto-mount"))
        valent_sftp_plugin_sftp_request (self);
    }
  else if ((state & VALENT_DEVICE_STATE_PAIRED) == 0)
    {
      g_clear_pointer (&self->session, sftp_session_end);
    }
  else if ((state & VALENT_DEVICE_STATE_CONNECTED) == 0)
    {
      g_clear_pointer (&self->session, sftp_session_free);
    }
}

static void
valent_sftp_plugin_handle_packet (ValentDevicePlugin *plugin,
                                  const char         *type,
                                  JsonNode           *packet)
{
  ValentSftpPlugin *self = VALENT_SFTP_PLUGIN (plugin);

  g_assert (VALENT_IS_SFTP_PLUGIN (self));
  g_assert (type != NULL);
  g_assert (VALENT_IS_PACKET (packet));

  if (g_str_equal (type, "kdeconnect.sftp"))
    valent_sftp_plugin_handle_sftp (self, packet);

  else if (g_str_equal (type, "kdeconnect.sftp.request"))
    valent_sftp_plugin_handle_request (self, packet);

  else
    g_warn_if_reached ();
}

/*
 * ValentObject
 */
static void
valent_sftp_plugin_destroy (ValentObject *object)
{
  ValentSftpPlugin *self = VALENT_SFTP_PLUGIN (object);
  ValentDevicePlugin *plugin = VALENT_DEVICE_PLUGIN (object);

  /* Stop watching the volume monitor and unmount any current session */
  if (self->monitor != NULL)
    {
      g_signal_handlers_disconnect_by_data (self->monitor, self);
      g_clear_object (&self->monitor);
    }
  g_clear_pointer (&self->session, sftp_session_end);
  g_clear_object (&self->connection);

  valent_device_plugin_set_menu_item (plugin, "device.sftp.browse", NULL);

  VALENT_OBJECT_CLASS (valent_sftp_plugin_parent_class)->destroy (object);
}

/*
 * GObject
 */
static void
valent_sftp_plugin_constructed (GObject *object)
{
  ValentSftpPlugin *self = VALENT_SFTP_PLUGIN (object);
  ValentDevicePlugin *plugin = VALENT_DEVICE_PLUGIN (object);

  G_OBJECT_CLASS (valent_sftp_plugin_parent_class)->constructed (object);

  g_action_map_add_action_entries (G_ACTION_MAP (plugin),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   plugin);

  self->connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
  self->monitor = g_volume_monitor_get ();
  g_signal_connect_object (self->monitor,
                           "mount-added",
                           G_CALLBACK (on_mount_added),
                           self,
                           G_CONNECT_DEFAULT);
  g_signal_connect_object (self->monitor,
                           "mount-removed",
                           G_CALLBACK (on_mount_removed),
                           self,
                           G_CONNECT_DEFAULT);
}

static void
valent_sftp_plugin_class_init (ValentSftpPluginClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentObjectClass *vobject_class = VALENT_OBJECT_CLASS (klass);
  ValentDevicePluginClass *plugin_class = VALENT_DEVICE_PLUGIN_CLASS (klass);

  object_class->constructed = valent_sftp_plugin_constructed;

  vobject_class->destroy = valent_sftp_plugin_destroy;

  plugin_class->handle_packet = valent_sftp_plugin_handle_packet;
  plugin_class->update_state = valent_sftp_plugin_update_state;
}

static void
valent_sftp_plugin_init (ValentSftpPlugin *self)
{
}

