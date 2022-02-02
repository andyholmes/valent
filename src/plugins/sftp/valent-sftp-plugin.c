// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-sftp-plugin"

#include "config.h"

#include <glib/gi18n.h>
#include <libpeas/peas.h>
#include <libvalent-core.h>

#include "valent-sftp-plugin.h"


typedef struct _ValentSftpSession ValentSftpSession;

struct _ValentSftpPlugin
{
  PeasExtensionBase  parent_instance;

  ValentDevice      *device;
  GSettings         *settings;

  GVolumeMonitor    *monitor;
  ValentSftpSession *session;
};

static void valent_device_plugin_iface_init (ValentDevicePluginInterface *iface);

G_DEFINE_TYPE_WITH_CODE (ValentSftpPlugin, valent_sftp_plugin, PEAS_TYPE_EXTENSION_BASE,
                         G_IMPLEMENT_INTERFACE (VALENT_TYPE_DEVICE_PLUGIN, valent_device_plugin_iface_init))

enum {
  PROP_0,
  PROP_DEVICE,
  N_PROPERTIES
};


static char *
get_device_host (ValentSftpPlugin *self)
{
  g_autoptr (ValentChannel) channel = NULL;
  g_autofree char *host = NULL;
  GParamSpec *pspec = NULL;

  /* The plugin doesn't know ValentChannel derivations, so we have to check for
   * a "host" property to ensure it's IP-based */
  channel = valent_device_ref_channel (self->device);

  if G_LIKELY (channel != NULL)
    pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (channel), "host");

  if G_LIKELY (pspec != NULL)
    g_object_get (channel, "host", &host, NULL);

  return g_steal_pointer (&host);
}

/**
 * ValentSftpSession:
 * @host: Host name or IP
 * @port: Port
 * @username: Username (deprecated)
 * @password: Password (deprecated)
 * @mount: A #GMount for the session
 *
 * #ValentSftpSession is a simple representation of a SFTP session.
 */
typedef struct _ValentSftpSession
{
  char    *host;
  guint16  port;
  char    *username;
  char    *password;

  char    *uri;
  GMount  *mount;
} ValentSftpSession;


static ValentSftpSession *
sftp_session_new (ValentSftpPlugin *self,
                  JsonNode         *packet)
{
  ValentSftpSession *session;
  g_autofree char *host = NULL;
  gint64 port;
  const char *password;
  const char *username;

  /* Ultimately, these are the only packet fields we really need */
  if (!valent_packet_get_int (packet, "port", &port) ||
      (port < 0 || port > G_MAXUINT16))
    {
      g_warning ("%s(): expected \"port\" field holding a uint16", G_STRFUNC);
      return NULL;
    }

  if ((host = get_device_host (self)) == NULL)
    {
      g_warning ("%s(): failed to get host address", G_STRFUNC);
      return NULL;
    }

  // Create a session struct
  session = g_new0 (ValentSftpSession, 1);
  session->host = g_steal_pointer (&host);
  session->port = (guint16)port;

  if (valent_packet_get_string (packet, "user", &username))
    session->username = g_strdup (username);

  if (valent_packet_get_string (packet, "password", &password))
    session->password = g_strdup (password);

  // Gvfs
  session->uri = g_strdup_printf ("sftp://%s:%u/",
                                  session->host,
                                  session->port);

  return session;
}

static void
sftp_session_free (gpointer data)
{
  ValentSftpSession *session = data;

  g_clear_pointer (&session->host, g_free);
  g_clear_pointer (&session->username, g_free);
  g_clear_pointer (&session->password, g_free);

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
  g_autofree char *host_patt = NULL;
  g_autoptr (GRegex) regex = NULL;
  g_autolist (GMount) mounts = NULL;

  if (self->session && self->session->mount)
    return TRUE;

  if ((host = get_device_host (self)) == NULL)
    return FALSE;

  // TODO: is this reasonable?
  host_patt = g_strdup_printf ("sftp://(%s):([22-65535])", host);
  regex = g_regex_new (host_patt, G_REGEX_OPTIMIZE, 0, NULL);

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

          g_clear_object (&self->session->mount);
          self->session->mount = g_object_ref (G_MOUNT (iter->data));

          g_clear_pointer (&self->session->uri, g_free);
          self->session->uri = g_steal_pointer (&uri);

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
    g_set_object (&self->session->mount, mount);
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
    g_clear_pointer (&self->session, sftp_session_free);
}


/* GMountOperation Callbacks
 *
 * Rather than setting the password ahead of time, we set it upon request to
 * avoid password authentication if possible.
 *
 * All host keys are accepted since we connect to known hosts as communicated
 * over the TLS encrypted #ValentLanChannel.
 */
static void
ask_password_cb (GMountOperation   *op,
                 char              *message,
                 char              *default_user,
                 char              *default_domain,
                 GAskPasswordFlags  flags,
                 gpointer           user_data)
{
  ValentSftpSession *session = user_data;

  if (flags & G_ASK_PASSWORD_NEED_USERNAME)
    g_mount_operation_set_username (op, session->username);

  if (flags & G_ASK_PASSWORD_NEED_PASSWORD)
    {
      g_mount_operation_set_password (op, session->password);
      g_mount_operation_set_password_save (op, G_PASSWORD_SAVE_NEVER);
    }

  g_mount_operation_reply (op, G_MOUNT_OPERATION_HANDLED);
}

static void
ask_question_cb (GMountOperation *op,
                 char            *message,
                 GStrv            choices,
                 gpointer         user_data)
{
  g_mount_operation_reply (op, G_MOUNT_OPERATION_HANDLED);
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

  for (guint16 port = 1739; port <= 1764; port++)
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
 * Packet Handlers
 */
static void
mount_cb (GFile            *file,
          GAsyncResult     *result,
          ValentSftpPlugin *self)
{
  g_autoptr (GError) error = NULL;

  g_assert (VALENT_IS_SFTP_PLUGIN (self));

  /* On success we will acquire the mount from the volume monitor */
  if (g_file_mount_enclosing_volume_finish (file, result, &error))
    return;

  /* On the off-chance this happens, we will just ensure we have the mount */
  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_ALREADY_MOUNTED) &&
      sftp_session_find (self))
    return;

  /* On failure, we're particularly interested in host key failures so that
   * we can remove those from the ssh-agent. These are reported by gvfs as
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
sshadd_cb (GSubprocess      *proc,
           GAsyncResult     *result,
           ValentSftpPlugin *self)
{
  ValentSftpSession *session = self->session;
  g_autoptr (GError) error = NULL;
  g_autoptr (GFile) file = NULL;
  g_autoptr (GMountOperation) op = NULL;

  g_assert (self->session != NULL);

  if (!g_subprocess_wait_check_finish (proc, result, &error))
    {
      g_warning ("%s(): Failed to add host key: %s", G_STRFUNC, error->message);
      g_clear_pointer (&self->session, sftp_session_free);
      return;
    }

  /* Prepare the mount operation */
  op = g_mount_operation_new ();
  g_signal_connect (op, "ask-password", G_CALLBACK (ask_password_cb), session);
  g_signal_connect (op, "ask-question", G_CALLBACK (ask_question_cb), NULL);

  /* Start the mount operation */
  file = g_file_new_for_uri (session->uri);
  g_file_mount_enclosing_volume (file,
                                 G_MOUNT_MOUNT_NONE,
                                 op,
                                 NULL,
                                 (GAsyncReadyCallback)mount_cb,
                                 self);
}

static void
sftp_session_begin (ValentSftpPlugin  *self,
                    ValentSftpSession *session)
{
  g_autoptr (GSubprocess) proc = NULL;
  g_autoptr (GError) error = NULL;
  g_autoptr (ValentData) data = NULL;
  ValentData *root_data;
  g_autofree char *path = NULL;

  g_assert (VALENT_IS_SFTP_PLUGIN (self));

  /* Get the manager's data object */
  data = valent_device_ref_data (self->device);
  root_data = valent_data_get_parent (data);

  /* Add the private key to the ssh-agent */
  path = g_build_filename (valent_data_get_config_path (root_data),
                           "private.pem",
                           NULL);
  proc = g_subprocess_new (G_SUBPROCESS_FLAGS_STDOUT_SILENCE |
                           G_SUBPROCESS_FLAGS_STDERR_MERGE,
                           &error,
                           "ssh-add", path,
                           NULL);

  if (proc == NULL)
    {
      g_warning ("%s(): Failed to add host key: %s", G_STRFUNC, error->message);
      g_clear_pointer (&self->session, sftp_session_free);
      return;
    }

  g_subprocess_wait_check_async (proc,
                                 NULL,
                                 (GAsyncReadyCallback)sshadd_cb,
                                 self);
}


/*
 * Packet Handlers
 */
static void
handle_sftp_error (ValentSftpPlugin *self,
                   JsonNode         *packet)
{
  g_autoptr (GNotification) notification = NULL;
  g_autoptr (GIcon) error_icon = NULL;
  g_autofree char *error_title = NULL;
  const char *error_message;
  const char *device_name;
  PeasPluginInfo *info;
  const char *plugin_name;
  JsonObject *body;

  g_assert (VALENT_IS_SFTP_PLUGIN (self));

  body = valent_packet_get_body (packet);

  info = peas_extension_base_get_plugin_info (PEAS_EXTENSION_BASE (self));
  plugin_name = peas_plugin_info_get_name (info);
  device_name = valent_device_get_name (self->device);

  error_icon = g_themed_icon_new ("dialog-error-symbolic");
  error_title = g_strdup_printf ("%s: %s", device_name, plugin_name);
  error_message = json_object_get_string_member (body, "errorMessage");

  /* Send notification */
  notification = g_notification_new (error_title);
  g_notification_set_body (notification, error_message);
  g_notification_set_icon (notification, error_icon);
  g_notification_set_priority (notification, G_NOTIFICATION_PRIORITY_HIGH);
  valent_device_show_notification (self->device, "sftp-error", notification);
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
    sftp_session_begin (self, self->session);
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
  JsonBuilder *builder;
  g_autoptr (JsonNode) response = NULL;

  g_assert (VALENT_IS_SFTP_PLUGIN (self));

  if (!valent_packet_check_field (packet, "startBrowsing"))
    return;

  builder = valent_packet_start ("kdeconnect.sftp");

  if (g_settings_get_boolean (self->settings, "local-allow"))
    {
      guint16 local_port;

      json_builder_set_member_name (builder, "user");
      json_builder_add_string_value (builder, g_get_user_name ());

      local_port = g_settings_get_uint (self->settings, "local-port");
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

  response = valent_packet_finish (builder);
  valent_device_queue_packet (self->device, response);
}

static void
valent_sftp_plugin_sftp_request (ValentSftpPlugin *self)
{
  JsonBuilder *builder;
  g_autoptr (JsonNode) packet = NULL;

  g_assert (VALENT_IS_SFTP_PLUGIN (self));

  if (sftp_session_find (self))
    return;

  builder = valent_packet_start ("kdeconnect.sftp.request");
  json_builder_set_member_name (builder, "startBrowsing");
  json_builder_add_boolean_value (builder, TRUE);
  packet = valent_packet_finish (builder);

  valent_device_queue_packet (self->device, packet);
}

/*
 * GActions
 */
static void
mount_action (GSimpleAction *action,
              GVariant      *parameter,
              gpointer       user_data)
{
  ValentSftpPlugin *self = VALENT_SFTP_PLUGIN (user_data);

  g_assert (VALENT_IS_SFTP_PLUGIN (self));

  if (self->session != NULL && self->session->uri != NULL)
    g_app_info_launch_default_for_uri_async (self->session->uri,
                                             NULL, NULL, NULL, NULL);
  else
    valent_sftp_plugin_sftp_request (self);
}

static const GActionEntry actions[] = {
    {"sftp-browse", mount_action, NULL, NULL, NULL}
};

static const ValentMenuEntry items[] = {
    {N_("Browse Files"), "device.sftp-browse", "folder-remote-symbolic"}
};


/*
 * ValentDevicePlugin
 */
static void
valent_sftp_plugin_enable (ValentDevicePlugin *plugin)
{
  ValentSftpPlugin *self = VALENT_SFTP_PLUGIN (plugin);
  const char *device_id;

  g_assert (VALENT_IS_SFTP_PLUGIN (self));

  device_id = valent_device_get_id (self->device);
  self->settings = valent_device_plugin_new_settings (device_id, "sftp");

  valent_device_plugin_register_actions (plugin,
                                         actions,
                                         G_N_ELEMENTS (actions));
  valent_device_plugin_add_menu_entries (plugin,
                                         items,
                                         G_N_ELEMENTS (items));

  /* Watch the volume monitor */
  self->monitor = g_volume_monitor_get ();
  g_signal_connect (self->monitor,
                    "mount-added",
                    G_CALLBACK (on_mount_added),
                    self);
  g_signal_connect (self->monitor,
                    "mount-removed",
                    G_CALLBACK (on_mount_removed),
                    self);
}

static void
valent_sftp_plugin_disable (ValentDevicePlugin *plugin)
{
  ValentSftpPlugin *self = VALENT_SFTP_PLUGIN (plugin);

  g_assert (VALENT_IS_SFTP_PLUGIN (self));

  /* Stop watching the volume monitor and unmount any current session */
  g_signal_handlers_disconnect_by_data (self->monitor, self);
  g_clear_object (&self->monitor);
  g_clear_pointer (&self->session, sftp_session_end);

  valent_device_plugin_remove_menu_entries (plugin,
                                            items,
                                            G_N_ELEMENTS (items));
  valent_device_plugin_unregister_actions (plugin,
                                           actions,
                                           G_N_ELEMENTS (actions));
  g_clear_object (&self->settings);
}

static void
valent_sftp_plugin_update_state (ValentDevicePlugin *plugin,
                                 ValentDeviceState   state)
{
  ValentSftpPlugin *self = VALENT_SFTP_PLUGIN (plugin);
  gboolean available;

  g_assert (VALENT_IS_SFTP_PLUGIN (self));

  available = (state & VALENT_DEVICE_STATE_CONNECTED) != 0 &&
              (state & VALENT_DEVICE_STATE_PAIRED) != 0;

  valent_device_plugin_toggle_actions (plugin,
                                       actions,
                                       G_N_ELEMENTS (actions),
                                       available);

  /* GMounts */
  if (available)
    {
      sftp_session_find (self);

      if (g_settings_get_boolean (self->settings, "auto-mount"))
        valent_sftp_plugin_sftp_request (self);
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

  if (g_strcmp0 (type, "kdeconnect.sftp") == 0)
    valent_sftp_plugin_handle_sftp (self, packet);

  else if (g_strcmp0 (type, "kdeconnect.sftp.request") == 0)
    valent_sftp_plugin_handle_request (self, packet);

  else
    g_warn_if_reached ();
}

static void
valent_device_plugin_iface_init (ValentDevicePluginInterface *iface)
{
  iface->enable = valent_sftp_plugin_enable;
  iface->disable = valent_sftp_plugin_disable;
  iface->handle_packet = valent_sftp_plugin_handle_packet;
  iface->update_state = valent_sftp_plugin_update_state;
}

/*
 * GObject
 */
static void
valent_sftp_plugin_dispose (GObject *object)
{
  ValentSftpPlugin *self = VALENT_SFTP_PLUGIN (object);

  g_clear_pointer (&self->session, sftp_session_free);

  G_OBJECT_CLASS (valent_sftp_plugin_parent_class)->dispose (object);
}

static void
valent_sftp_plugin_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  ValentSftpPlugin *self = VALENT_SFTP_PLUGIN (object);

  switch (prop_id)
    {
    case PROP_DEVICE:
      g_value_set_object (value, self->device);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_sftp_plugin_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  ValentSftpPlugin *self = VALENT_SFTP_PLUGIN (object);

  switch (prop_id)
    {
    case PROP_DEVICE:
      self->device = g_value_get_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_sftp_plugin_class_init (ValentSftpPluginClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = valent_sftp_plugin_dispose;
  object_class->get_property = valent_sftp_plugin_get_property;
  object_class->set_property = valent_sftp_plugin_set_property;

  g_object_class_override_property (object_class, PROP_DEVICE, "device");
}

static void
valent_sftp_plugin_init (ValentSftpPlugin *self)
{
}

