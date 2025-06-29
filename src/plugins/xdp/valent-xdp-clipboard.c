// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-xdp-clipboard"

#include "config.h"

#include <gio/gio.h>
#include <gio/gunixinputstream.h>
#include <gio/gunixoutputstream.h>
#include <valent.h>

#include "valent-xdp-clipboard.h"

#define CLIPBOARD_IFACE              "org.freedesktop.portal.Clipboard"
#define PORTAL_DESKTOP_NAME          "org.freedesktop.portal.Desktop"
#define PORTAL_DESKTOP_PATH          "/org/freedesktop/portal/desktop"
#define REMOTE_DESKTOP_IFACE         "org.freedesktop.portal.RemoteDesktop"

#define REQUEST_IFACE                "org.freedesktop.portal.Request"
#define REQUEST_PATH_PREFIX          "/org/freedesktop/portal/desktop/request/"
#define SESSION_PATH_PREFIX          "/org/freedesktop/portal/desktop/session/"

#define CLIPBOARD_MAXSIZE (16 * 1024)


struct _ValentXdpClipboard
{
  ValentClipboardAdapter  parent_instance;

  GDBusConnection        *connection;
  char                   *sender;
  char                   *session_handle;
  char                   *create_request;
  char                   *start_request;
  unsigned int            closed_id;
  unsigned int            response_id;
  unsigned int            selection_owner_changed_id;
  unsigned int            selection_transfer_id;
  unsigned int            watcher_id;

  GBytes                 *content;
  GVariant               *mimetypes;
  int64_t                 timestamp;
  gboolean                is_owner;
};

G_DEFINE_FINAL_TYPE (ValentXdpClipboard, valent_xdp_clipboard, VALENT_TYPE_CLIPBOARD_ADAPTER);


static int
unix_fd_list_get (GUnixFDList  *list,
                  int           index_,
                  GError      **error)
{
  int fd;
  int flags;

  g_assert (G_IS_UNIX_FD_LIST (list));

  if ((fd = g_unix_fd_list_get (list, index_, error)) == -1)
    return -1;

  if ((flags = fcntl (fd, F_GETFD)) == -1)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   g_io_error_from_errno (errno),
                   "fcntl: %s", g_strerror (errno));
      close (fd);
      return -1;
    }

  if (fcntl (fd, F_SETFD, flags | FD_CLOEXEC) == -1)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   g_io_error_from_errno (errno),
                   "fcntl: %s", g_strerror (errno));
      close (fd);
      return -1;
    }

  return fd;
}

/*< private >
 * SelectionData:
 * @content: the data being transferred
 * @mimetype: the data format
 * @fd: the file descriptor
 * @serial: the transfer ID
 *
 * A `struct` for clipboard transfer data.
 */
typedef struct
{
  GBytes   *content;
  char     *mimetype;
  int       fd;
  uint32_t  serial;
} SelectionData;

static void
selection_data_free (gpointer data)
{
  SelectionData *selection = (SelectionData *)data;

  g_clear_pointer (&selection->content, g_bytes_unref);
  g_clear_pointer (&selection->mimetype, g_free);
  g_clear_pointer (&selection, g_free);
}

/*
 * Read
 */
static void
g_input_stream_read_bytes_cb (GInputStream *stream,
                              GAsyncResult *result,
                              gpointer      user_data)
{
  g_autoptr (GTask) task = G_TASK (user_data);
  g_autoptr (GBytes) bytes = NULL;
  GError *error = NULL;

  bytes = g_input_stream_read_bytes_finish (stream, result, &error);
  if (bytes == NULL)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  g_task_return_pointer (task,
                         g_steal_pointer (&bytes),
                         (GDestroyNotify)g_bytes_unref);
}

static void
selection_read_cb (GDBusConnection *connection,
                   GAsyncResult    *result,
                   gpointer         user_data)
{
  g_autoptr (GTask) task = G_TASK (user_data);
  GCancellable *cancellable = g_task_get_cancellable (task);
  g_autoptr (GVariant) reply = NULL;
  g_autoptr (GInputStream) stream = NULL;
  g_autoptr (GUnixFDList) fd_list = NULL;
  int index_;
  int fd;
  GError *error = NULL;

  reply = g_dbus_connection_call_with_unix_fd_list_finish (connection,
                                                           &fd_list,
                                                           result,
                                                           &error);
  if (reply == NULL)
    {
      g_dbus_error_strip_remote_error (error);
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  g_variant_get (reply, "(h)", &index_);
  if ((fd = unix_fd_list_get (fd_list, index_, &error)) == -1)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  stream = g_unix_input_stream_new (fd, TRUE);
  g_input_stream_read_bytes_async (stream,
                                   CLIPBOARD_MAXSIZE,
                                   G_PRIORITY_DEFAULT,
                                   cancellable,
                                   (GAsyncReadyCallback)g_input_stream_read_bytes_cb,
                                   g_steal_pointer (&task));
}

/*
 * Write
 */
static void
selection_transfer_cb (GObject      *object,
                       GAsyncResult *result,
                       gpointer      user_data)
{
  g_autoptr (GError) error = NULL;

  if (!g_task_propagate_boolean (G_TASK (result), &error) &&
      !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    g_warning ("%s: %s", G_OBJECT_TYPE_NAME (object), error->message);
}

static void
selection_write_done_cb (GDBusConnection *connection,
                         GAsyncResult    *result,
                         gpointer         user_data)
{
  g_autoptr (GVariant) reply = NULL;
  g_autoptr (GError) error = NULL;

  reply = g_dbus_connection_call_finish (connection, result, &error);
  if (reply == NULL)
    {
      g_dbus_error_strip_remote_error (error);
      g_warning ("%s(): %s", G_STRFUNC, error->message);
    }
}

static void
g_output_stream_write_bytes_cb (GOutputStream *stream,
                                GAsyncResult  *result,
                                gpointer       user_data)
{
  g_autoptr (GTask) task = G_TASK (user_data);
  ValentXdpClipboard *self = g_task_get_source_object (task);
  GCancellable *cancellable = g_task_get_cancellable (task);
  SelectionData *selection = g_task_get_task_data (task);
  gboolean success = FALSE;
  GError *error = NULL;

  success = g_output_stream_write_bytes_finish (stream, result, &error) != -1;
  g_dbus_connection_call (self->connection,
                          PORTAL_DESKTOP_NAME,
                          PORTAL_DESKTOP_PATH,
                          CLIPBOARD_IFACE,
                          "SelectionWriteDone",
                          g_variant_new ("(oub)",
                                         self->session_handle,
                                         selection->serial,
                                         success),
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          cancellable,
                          (GAsyncReadyCallback)selection_write_done_cb,
                          NULL);

  if (!success)
    g_task_return_error (task, g_steal_pointer (&error));
  else
    g_task_return_boolean (task, TRUE);
}

static void
selection_write_cb (GDBusConnection *connection,
                    GAsyncResult    *result,
                    gpointer         user_data)
{
  g_autoptr (GTask) task = G_TASK (user_data);
  GCancellable *cancellable = g_task_get_cancellable (task);
  SelectionData *selection = g_task_get_task_data (task);
  g_autoptr (GVariant) reply = NULL;
  g_autoptr (GOutputStream) stream = NULL;
  g_autoptr (GUnixFDList) fd_list = NULL;
  int index_;
  GError *error = NULL;

  reply = g_dbus_connection_call_with_unix_fd_list_finish (connection,
                                                           &fd_list,
                                                           result,
                                                           &error);
  if (reply == NULL)
    {
      g_dbus_error_strip_remote_error (error);
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  g_variant_get (reply, "(h)", &index_);
  if ((selection->fd = unix_fd_list_get (fd_list, index_, &error)) == -1)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  stream = g_unix_output_stream_new (selection->fd, TRUE);
  g_output_stream_write_bytes_async (stream,
                                     selection->content,
                                     G_PRIORITY_DEFAULT,
                                     cancellable,
                                     (GAsyncReadyCallback)g_output_stream_write_bytes_cb,
                                     g_object_ref (task));
}

static void
valent_xdp_clipboard_selection_write (ValentXdpClipboard *self,
                                      const char         *mimetype,
                                      uint32_t            serial)
{
  g_autoptr (GTask) task = NULL;
  g_autoptr (GCancellable) cancellable = NULL;
  SelectionData *selection = NULL;

  g_assert (VALENT_IS_XDP_CLIPBOARD (self));

  selection = g_new0 (SelectionData, 1);
  selection->content = g_bytes_ref (self->content);
  selection->mimetype = g_strdup (mimetype);
  selection->serial = serial;

  cancellable = valent_object_ref_cancellable (VALENT_OBJECT (self));

  task = g_task_new (self, cancellable, selection_transfer_cb, NULL);
  g_task_set_task_data (task, selection, selection_data_free);
  g_task_set_source_tag (task, valent_xdp_clipboard_selection_write);

  g_dbus_connection_call (self->connection,
                          PORTAL_DESKTOP_NAME,
                          PORTAL_DESKTOP_PATH,
                          CLIPBOARD_IFACE,
                          "SelectionWrite",
                          g_variant_new ("(ou)",
                                         self->session_handle,
                                         selection->serial),
                          G_VARIANT_TYPE ("(oh)"),
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          cancellable,
                          (GAsyncReadyCallback)selection_write_cb,
                          g_object_ref (task));
}

/*
 * org.gnome.Xdp.RemoteDesktop.Session Callbacks
 */
static void
on_closed (GDBusConnection *connection,
           const char      *sender_name,
           const char      *object_path,
           const char      *interface_name,
           const char      *signal_name,
           GVariant        *parameters,
           gpointer         user_data)
{
  ValentXdpClipboard *self = VALENT_XDP_CLIPBOARD (user_data);

  g_assert (VALENT_IS_XDP_CLIPBOARD (self));
  g_assert (g_str_equal (signal_name, "Closed"));

  if (self->closed_id != 0)
    {
      g_dbus_connection_signal_unsubscribe (connection,
                                            self->closed_id);
      self->closed_id = 0;
    }

  if (self->selection_owner_changed_id != 0)
    {
      g_dbus_connection_signal_unsubscribe (connection,
                                            self->selection_owner_changed_id);
      self->selection_owner_changed_id = 0;
    }

  if (self->selection_transfer_id != 0)
    {
      g_dbus_connection_signal_unsubscribe (connection,
                                            self->selection_transfer_id);
      self->selection_transfer_id = 0;
    }

  g_clear_pointer (&self->session_handle, g_free);
}

static void
on_selection_owner_changed (GDBusConnection *connection,
                            const char      *sender_name,
                            const char      *object_path,
                            const char      *interface_name,
                            const char      *signal_name,
                            GVariant        *parameters,
                            gpointer         user_data)
{
  ValentClipboardAdapter *adapter = VALENT_CLIPBOARD_ADAPTER (user_data);
  ValentXdpClipboard *self = VALENT_XDP_CLIPBOARD (user_data);
  const char *session_handle = NULL;
  g_autoptr (GVariant) options = NULL;
  g_autoptr (GVariant) mimetypes = NULL;

  g_assert (VALENT_IS_CLIPBOARD_ADAPTER (adapter));
  g_assert (g_str_equal (signal_name, "SelectionOwnerChanged"));

  g_variant_get (parameters, "(&o@a{sv})", &session_handle, &options);
  if (g_strcmp0 (self->session_handle, session_handle) != 0)
    return;

  if (g_variant_lookup (options, "mime_types", "(@as)", &mimetypes))
    {
      g_clear_pointer (&self->mimetypes, g_variant_unref);
      self->mimetypes = g_variant_ref_sink (g_steal_pointer (&mimetypes));
    }

  if (!g_variant_lookup (options, "session_is_owner", "b", &self->is_owner))
    self->is_owner = FALSE;

  /* Free the cache if ownership of the selection has been lost
   */
  if (!self->is_owner)
    {
      g_clear_pointer (&self->content, g_bytes_unref);
      self->timestamp = valent_timestamp_ms ();
    }

  valent_clipboard_adapter_changed (adapter);
}

static void
on_selection_transfer (GDBusConnection *connection,
                       const char      *sender_name,
                       const char      *object_path,
                       const char      *interface_name,
                       const char      *signal_name,
                       GVariant        *parameters,
                       gpointer         user_data)
{
  ValentXdpClipboard *self = VALENT_XDP_CLIPBOARD (user_data);
  const char *session_handle;
  const char *mimetype;
  uint32_t serial;

  g_assert (VALENT_IS_XDP_CLIPBOARD (self));
  g_assert (g_str_equal (signal_name, "SelectionTransfer"));

  g_variant_get (parameters, "(&o&su)", &session_handle, &mimetype, &serial);
  if (g_strcmp0 (self->session_handle, session_handle) != 0)
    return;

  valent_xdp_clipboard_selection_write (self, mimetype, serial);
}

/*
 * ValentClipboardAdapter
 */
static GStrv
valent_xdp_clipboard_get_mimetypes (ValentClipboardAdapter *adapter)
{
  ValentXdpClipboard *self = VALENT_XDP_CLIPBOARD (adapter);

  g_assert (VALENT_IS_XDP_CLIPBOARD (self));

  if (self->mimetypes == NULL)
    return NULL;

  return g_variant_dup_strv (self->mimetypes, NULL);
}

static int64_t
valent_xdp_clipboard_get_timestamp (ValentClipboardAdapter *adapter)
{
  ValentXdpClipboard *self = VALENT_XDP_CLIPBOARD (adapter);

  g_assert (VALENT_IS_XDP_CLIPBOARD (self));

  return self->timestamp;
}

static void
valent_xdp_clipboard_read_bytes (ValentClipboardAdapter *adapter,
                                 const char             *mimetype,
                                 GCancellable           *cancellable,
                                 GAsyncReadyCallback     callback,
                                 gpointer                user_data)
{
  ValentXdpClipboard *self = VALENT_XDP_CLIPBOARD (adapter);
  g_autoptr (GTask) task = NULL;
  g_autofree const char **mimetypes = NULL;

  g_assert (VALENT_IS_XDP_CLIPBOARD (self));
  g_assert (mimetype != NULL && *mimetype != '\0');

  if (self->connection == NULL || self->session_handle == NULL)
    {
      g_task_report_new_error (adapter, callback, user_data, callback,
                               G_IO_ERROR,
                               G_IO_ERROR_DBUS_ERROR,
                               "Clipboard service not available.");
      return;
    }

  if (self->mimetypes == NULL)
    {
      g_task_report_new_error (adapter, callback, user_data, callback,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_SUPPORTED,
                               "Clipboard empty");
      return;
    }

  mimetypes = g_variant_get_strv (self->mimetypes, NULL);
  if (!g_strv_contains (mimetypes, mimetype))
    {
      g_task_report_new_error (adapter, callback, user_data, callback,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_SUPPORTED,
                               "%s format not available.",
                               mimetype);
      return;
    }

  task = g_task_new (adapter, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_xdp_clipboard_read_bytes);

  if (self->is_owner && self->content != NULL)
    {
      g_task_return_pointer (task,
                             g_bytes_ref (self->content),
                             (GDestroyNotify)g_bytes_unref);
      return;
    }

  g_dbus_connection_call (self->connection,
                          PORTAL_DESKTOP_NAME,
                          PORTAL_DESKTOP_PATH,
                          CLIPBOARD_IFACE,
                          "SelectionRead",
                          g_variant_new ("(os)", self->session_handle, mimetype),
                          G_VARIANT_TYPE ("(h)"),
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          cancellable,
                          (GAsyncReadyCallback)selection_read_cb,
                          g_object_ref (task));
}

static void
set_selection_cb (GDBusConnection *connection,
                  GAsyncResult    *result,
                  gpointer         user_data)
{
  g_autoptr (GTask) task = G_TASK (user_data);
  g_autoptr (GVariant) reply = NULL;
  GError *error = NULL;

  reply = g_dbus_connection_call_finish (connection, result, &error);
  if (reply == NULL)
    {
      g_dbus_error_strip_remote_error (error);
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  g_task_return_boolean (task, TRUE);
}

static void
valent_xdp_clipboard_write_bytes (ValentClipboardAdapter *adapter,
                                  const char             *mimetype,
                                  GBytes                 *bytes,
                                  GCancellable           *cancellable,
                                  GAsyncReadyCallback     callback,
                                  gpointer                user_data)
{
  ValentXdpClipboard *self = VALENT_XDP_CLIPBOARD (adapter);
  g_autoptr (GTask) task = NULL;
  GVariantBuilder options;

  g_assert (VALENT_IS_XDP_CLIPBOARD (self));
  g_assert (bytes != NULL || (mimetype != NULL && *mimetype != '\0'));

  if (self->connection == NULL || self->session_handle == NULL)
    {
      g_task_report_new_error (adapter, callback, user_data, callback,
                               G_IO_ERROR,
                               G_IO_ERROR_DBUS_ERROR,
                               "Clipboard service not available.");
      return;
    }

  /* Update the local content */
  g_clear_pointer (&self->content, g_bytes_unref);
  self->content = g_bytes_ref (bytes);
  g_clear_pointer (&self->mimetypes, g_variant_unref);
  self->mimetypes = g_variant_new_strv (VALENT_STRV_INIT (mimetype), -1);
  g_variant_ref_sink (self->mimetypes);
  self->timestamp = valent_timestamp_ms ();

  /* Inform Xdp */
  task = g_task_new (adapter, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_xdp_clipboard_write_bytes);

  g_variant_builder_init (&options, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add (&options, "{sv}", "mime_types", self->mimetypes);
  g_dbus_connection_call (self->connection,
                          PORTAL_DESKTOP_NAME,
                          PORTAL_DESKTOP_PATH,
                          CLIPBOARD_IFACE,
                          "SetSelection",
                          g_variant_new ("(@a{sv})",
                                         g_variant_builder_end (&options)),
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          cancellable,
                          (GAsyncReadyCallback)set_selection_cb,
                          g_object_ref (task));
}

/*
 * GAsyncInitable
 */
static void
portal_call_cb (GDBusConnection *connection,
                GAsyncResult    *result,
                gpointer         user_data)
{
  ValentXdpClipboard *self = VALENT_XDP_CLIPBOARD (user_data);
  g_autoptr (GVariant) reply = NULL;
  g_autoptr (GError) error = NULL;

  reply = g_dbus_connection_call_finish (connection, result, &error);
  if (reply != NULL)
    {
      valent_extension_plugin_state_changed (VALENT_EXTENSION (self),
                                             VALENT_PLUGIN_STATE_ACTIVE,
                                             NULL);
    }
  else if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
      valent_extension_plugin_state_changed (VALENT_EXTENSION (self),
                                             VALENT_PLUGIN_STATE_INACTIVE,
                                             NULL);
    }
  else
    {
      g_dbus_error_strip_remote_error (error);
      valent_extension_plugin_state_changed (VALENT_EXTENSION (self),
                                             VALENT_PLUGIN_STATE_ERROR,
                                             error);
    }
}

static void
on_start_response (GDBusConnection *connection,
                   const char      *sender_name,
                   const char      *object_path,
                   const char      *interface_name,
                   const char      *signal_name,
                   GVariant        *parameters,
                   gpointer         user_data)
{
  ValentXdpClipboard *self = VALENT_XDP_CLIPBOARD (user_data);
  uint32_t response;
  g_autoptr (GVariant) results = NULL;

  /* g_assert (VALENT_IS_XDP_CLIPBOARD (self)); */
  g_assert (g_str_equal (signal_name, "Response"));

  if (self->response_id != 0)
    {
      g_dbus_connection_signal_unsubscribe (self->connection,
                                            self->response_id);
      self->response_id = 0;
    }

  g_variant_get (parameters, "(u@a{sv})", &response, &results);
  if (response != 0)
    {
      g_debug ("%s(): Unexpected response: %u", G_STRFUNC, response);
      return;
    }
}

static void
request_clipboard_cb (GDBusConnection *connection,
                      GAsyncResult    *result,
                      gpointer         user_data)
{
  ValentXdpClipboard *self = g_steal_pointer (&user_data);
  g_autoptr (GVariant) reply = NULL;
  g_autofree char *token = NULL;
  g_autofree char *request_path = NULL;
  GVariantBuilder options;
  g_autoptr (GError) error = NULL;

  reply = g_dbus_connection_call_finish (connection, result, &error);
  if (reply == NULL)
    {
      g_dbus_error_strip_remote_error (error);
      g_warning ("%s(): %s", G_STRFUNC, error->message);
      return;
    }

  self->closed_id =
    g_dbus_connection_signal_subscribe (connection,
                                        PORTAL_DESKTOP_NAME,
                                        REMOTE_DESKTOP_IFACE,
                                        "Closed",
                                        self->session_handle,
                                        NULL,
                                        G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
                                        on_closed,
                                        self, NULL);
  self->selection_owner_changed_id =
    g_dbus_connection_signal_subscribe (connection,
                                        PORTAL_DESKTOP_NAME,
                                        CLIPBOARD_IFACE,
                                        "SelectionOwnerChanged",
                                        PORTAL_DESKTOP_PATH,
                                        NULL,
                                        G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
                                        on_selection_owner_changed,
                                        self, NULL);
  self->selection_transfer_id =
    g_dbus_connection_signal_subscribe (connection,
                                        PORTAL_DESKTOP_NAME,
                                        CLIPBOARD_IFACE,
                                        "SelectionTransfer",
                                        PORTAL_DESKTOP_PATH,
                                        NULL,
                                        G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
                                        on_selection_transfer,
                                        self, NULL);

  token = g_strdup_printf ("valent%d", g_random_int_range (0, G_MAXINT));
  self->start_request = g_strconcat (REQUEST_PATH_PREFIX, self->sender, "/", token, NULL);

  self->response_id =
    g_dbus_connection_signal_subscribe (connection,
                                        PORTAL_DESKTOP_NAME,
                                        REQUEST_IFACE,
                                        "Response",
                                        self->start_request,
                                        NULL,
                                        G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
                                        on_start_response,
                                        self, NULL);

  g_variant_builder_init (&options, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add (&options, "{sv}", "handle_token", g_variant_new_string (token));
  g_dbus_connection_call (self->connection,
                          PORTAL_DESKTOP_NAME,
                          PORTAL_DESKTOP_PATH,
                          REMOTE_DESKTOP_IFACE,
                          "Start",
                          g_variant_new ("(osa{sv})",
                                         self->session_handle,
                                         "", // parent_handle
                                         &options),
                          NULL,
                          G_DBUS_CALL_FLAGS_NO_AUTO_START,
                          -1,
                          g_task_get_cancellable (G_TASK (result)),
                          (GAsyncReadyCallback)portal_call_cb,
                          self);
}

static void
on_create_response (GDBusConnection *connection,
                    const char      *sender_name,
                    const char      *object_path,
                    const char      *interface_name,
                    const char      *signal_name,
                    GVariant        *parameters,
                    gpointer         user_data)
{
  ValentXdpClipboard *self = VALENT_XDP_CLIPBOARD (user_data);
  uint32_t response;
  g_autoptr (GVariant) results = NULL;

  /* g_assert (VALENT_IS_XDP_CLIPBOARD (self)); */
  g_assert (g_str_equal (signal_name, "Response"));

  if (self->response_id != 0)
    {
      g_dbus_connection_signal_unsubscribe (self->connection,
                                            self->response_id);
      self->response_id = 0;
    }

  g_variant_get (parameters, "(u@a{sv})", &response, &results);
  if (response != 0)
    {
      g_debug ("%s(): Unexpected response: %u", G_STRFUNC, response);
      return;
    }

  g_clear_pointer (&self->session_handle, g_free);
  if (g_variant_lookup (results, "session_handle", "s", &self->session_handle))
    {
      g_autoptr (GCancellable) cancellable = NULL;

      cancellable = valent_object_ref_cancellable (VALENT_OBJECT (self));
      g_dbus_connection_call (connection,
                              PORTAL_DESKTOP_NAME,
                              PORTAL_DESKTOP_PATH,
                              CLIPBOARD_IFACE,
                              "RequestClipboard",
                              g_variant_new_parsed ("(%o, @a{sv} {})",
                                                    self->session_handle),
                              NULL,
                              G_DBUS_CALL_FLAGS_NONE,
                              -1,
                              cancellable,
                              (GAsyncReadyCallback)request_clipboard_cb,
                              g_object_ref (self));
    }
}

static void
on_name_appeared (GDBusConnection    *connection,
                  const char         *name,
                  const char         *name_owner,
                  ValentXdpClipboard *self)
{
  GVariantBuilder options;
  g_autofree char *token = NULL;
  g_autofree char *session_token = NULL;
  g_autoptr (GCancellable) destroy = NULL;

  g_assert (VALENT_IS_XDP_CLIPBOARD (self));

  if (!g_set_object (&self->connection, connection))
    return;

  if (self->sender == NULL)
    {
      const char *sender = NULL;

      sender = g_dbus_connection_get_unique_name (connection);
      self->sender = g_strdup (sender + 1);
      for (size_t i = 0; self->sender[i]; i++)
        {
          if (self->sender[i] == '.')
            self->sender[i] = '_';
        }
    }

  destroy = valent_object_ref_cancellable (VALENT_OBJECT (self));
  token = g_strdup_printf ("valent%d", g_random_int_range (0, G_MAXINT));
  session_token = g_strdup_printf ("valent%d", g_random_int_range (0, G_MAXINT));

  self->create_request = g_strconcat (REQUEST_PATH_PREFIX, self->sender, "/", token, NULL);
  self->response_id =
    g_dbus_connection_signal_subscribe (connection,
                                        PORTAL_DESKTOP_NAME,
                                        REQUEST_IFACE,
                                        "Response",
                                        self->create_request,
                                        NULL,
                                        G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
                                        on_create_response,
                                        self, NULL);

  g_variant_builder_init (&options, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add (&options, "{sv}", "handle_token",
                         g_variant_new_string (token));
  g_variant_builder_add (&options, "{sv}", "session_handle_token",
                         g_variant_new_string (session_token));
  g_dbus_connection_call (self->connection,
                          PORTAL_DESKTOP_NAME,
                          PORTAL_DESKTOP_PATH,
                          REMOTE_DESKTOP_IFACE,
                          "CreateSession",
                          g_variant_new ("(a{sv})", &options),
                          G_VARIANT_TYPE ("(o)"),
                          G_DBUS_CALL_FLAGS_NO_AUTO_START,
                          -1,
                          destroy,
                          (GAsyncReadyCallback)portal_call_cb,
                          g_object_ref (self));
}

static void
on_name_vanished (GDBusConnection    *connection,
                  const char         *name,
                  ValentXdpClipboard *self)
{
  g_assert (VALENT_IS_XDP_CLIPBOARD (self));

  if (self->connection != NULL)
    {
      if (self->response_id != 0)
        {
          g_dbus_connection_signal_unsubscribe (self->connection,
                                                self->response_id);
          self->response_id = 0;
        }

      if (self->closed_id != 0)
        {
          g_dbus_connection_signal_unsubscribe (self->connection,
                                                self->closed_id);
          self->closed_id = 0;
        }

      if (self->selection_owner_changed_id != 0)
        {
          g_dbus_connection_signal_unsubscribe (self->connection,
                                                self->selection_owner_changed_id);
          self->selection_owner_changed_id = 0;
        }

      if (self->selection_transfer_id != 0)
        {
          g_dbus_connection_signal_unsubscribe (self->connection,
                                                self->selection_transfer_id);
          self->selection_transfer_id = 0;
        }

      if (self->session_handle != NULL)
        {
          g_dbus_connection_call (self->connection,
                                  PORTAL_DESKTOP_NAME,
                                  self->session_handle,
                                  REMOTE_DESKTOP_IFACE,
                                  "DisableClipboard",
                                  NULL,
                                  NULL,
                                  G_DBUS_CALL_FLAGS_NO_AUTO_START,
                                  -1,
                                  NULL,
                                  NULL,
                                  NULL);
          g_dbus_connection_call (self->connection,
                                  PORTAL_DESKTOP_NAME,
                                  self->session_handle,
                                  REMOTE_DESKTOP_IFACE,
                                  "Stop",
                                  NULL,
                                  NULL,
                                  G_DBUS_CALL_FLAGS_NO_AUTO_START,
                                  -1,
                                  NULL,
                                  NULL,
                                  NULL);
        }
    }

  g_clear_object (&self->connection);
  g_clear_pointer (&self->sender, g_free);
  g_clear_pointer (&self->create_request, g_free);
  g_clear_pointer (&self->start_request, g_free);
  g_clear_pointer (&self->session_handle, g_free);
  valent_extension_plugin_state_changed (VALENT_EXTENSION (self),
                                         VALENT_PLUGIN_STATE_INACTIVE,
                                         NULL);
}

/*
 * ValentObject
 */
static void
valent_xdp_clipboard_destroy (ValentObject *object)
{
  ValentXdpClipboard *self = VALENT_XDP_CLIPBOARD (object);

  g_clear_handle_id (&self->watcher_id, g_bus_unwatch_name);
  on_name_vanished (self->connection, PORTAL_DESKTOP_NAME, self);

  g_clear_pointer (&self->content, g_bytes_unref);
  g_clear_pointer (&self->mimetypes, g_variant_unref);

  VALENT_OBJECT_CLASS (valent_xdp_clipboard_parent_class)->destroy (object);
}

/*
 * GObject
 */
static void
valent_xdp_clipboard_constructed (GObject *object)
{
  ValentXdpClipboard *self = VALENT_XDP_CLIPBOARD (object);

  G_OBJECT_CLASS (valent_xdp_clipboard_parent_class)->constructed (object);

  self->watcher_id = g_bus_watch_name (G_BUS_TYPE_SESSION,
                                       PORTAL_DESKTOP_NAME,
                                       G_BUS_NAME_WATCHER_FLAGS_NONE,
                                       (GBusNameAppearedCallback)on_name_appeared,
                                       (GBusNameVanishedCallback)on_name_vanished,
                                       self, NULL);
}

static void
valent_xdp_clipboard_class_init (ValentXdpClipboardClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentObjectClass *vobject_class = VALENT_OBJECT_CLASS (klass);
  ValentClipboardAdapterClass *clipboard_class = VALENT_CLIPBOARD_ADAPTER_CLASS (klass);

  object_class->constructed = valent_xdp_clipboard_constructed;

  vobject_class->destroy = valent_xdp_clipboard_destroy;

  clipboard_class->get_mimetypes = valent_xdp_clipboard_get_mimetypes;
  clipboard_class->get_timestamp = valent_xdp_clipboard_get_timestamp;
  clipboard_class->read_bytes = valent_xdp_clipboard_read_bytes;
  clipboard_class->write_bytes = valent_xdp_clipboard_write_bytes;
}

static void
valent_xdp_clipboard_init (ValentXdpClipboard *self)
{
}

