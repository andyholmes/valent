// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-mutter-clipboard"

#include "config.h"

#include <gio/gio.h>
#include <gio/gunixinputstream.h>
#include <gio/gunixoutputstream.h>
#include <valent.h>

#include "valent-mutter-clipboard.h"

#define REMOTE_DESKTOP_NAME          "org.gnome.Mutter.RemoteDesktop"
#define REMOTE_DESKTOP_PATH          "/org/gnome/Mutter/RemoteDesktop"
#define REMOTE_DESKTOP_IFACE         "org.gnome.Mutter.RemoteDesktop"
#define REMOTE_DESKTOP_SESSION_IFACE "org.gnome.Mutter.RemoteDesktop.Session"

#define CLIPBOARD_MAXSIZE (16 * 1024)


struct _ValentMutterClipboard
{
  ValentClipboardAdapter  parent_instance;

  GDBusConnection        *connection;
  char                   *session_path;
  unsigned int            closed_id;
  unsigned int            selection_owner_changed_id;
  unsigned int            selection_transfer_id;
  unsigned int            watcher_id;

  GBytes                 *content;
  GVariant               *mimetypes;
  int64_t                 timestamp;
  gboolean                is_owner;
};

G_DEFINE_FINAL_TYPE (ValentMutterClipboard, valent_mutter_clipboard, VALENT_TYPE_CLIPBOARD_ADAPTER);


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
  g_autoptr (GError) error = NULL;

  bytes = g_input_stream_read_bytes_finish (stream, result, &error);

  if (bytes == NULL)
    g_task_return_error (task, g_steal_pointer (&error));

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
  GUnixFDList *list = NULL;
  int index_;
  int fd;
  g_autoptr (GError) error = NULL;

  reply = g_dbus_connection_call_with_unix_fd_list_finish (connection,
                                                           &list,
                                                           result,
                                                           &error);

  if (reply == NULL)
    {
      g_dbus_error_strip_remote_error (error);
      return g_task_return_error (task, g_steal_pointer (&error));
    }

  g_variant_get (reply, "(h)", &index_);

  if ((fd = unix_fd_list_get (list, index_, &error)) == -1)
    return g_task_return_error (task, g_steal_pointer (&error));

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
  ValentMutterClipboard *self = g_task_get_source_object (task);
  GCancellable *cancellable = g_task_get_cancellable (task);
  SelectionData *selection = g_task_get_task_data (task);
  gboolean success = FALSE;
  g_autoptr (GError) error = NULL;

  success = g_output_stream_write_bytes_finish (stream, result, &error) != -1;
  g_dbus_connection_call (self->connection,
                          REMOTE_DESKTOP_NAME,
                          self->session_path,
                          REMOTE_DESKTOP_SESSION_IFACE,
                          "SelectionWriteDone",
                          g_variant_new ("(ub)", selection->serial, success),
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          cancellable,
                          (GAsyncReadyCallback)selection_write_done_cb,
                          NULL);

  if (!success)
    return g_task_return_error (task, g_steal_pointer (&error));

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
  g_autoptr (GError) error = NULL;

  reply = g_dbus_connection_call_with_unix_fd_list_finish (connection,
                                                           &fd_list,
                                                           result,
                                                           &error);

  if (reply == NULL)
    {
      g_dbus_error_strip_remote_error (error);
      return g_task_return_error (task, g_steal_pointer (&error));
    }

  g_variant_get (reply, "(h)", &index_);

  if ((selection->fd = unix_fd_list_get (fd_list, index_, &error)) == -1)
    return g_task_return_error (task, g_steal_pointer (&error));

  stream = g_unix_output_stream_new (selection->fd, TRUE);
  g_output_stream_write_bytes_async (stream,
                                     selection->content,
                                     G_PRIORITY_DEFAULT,
                                     cancellable,
                                     (GAsyncReadyCallback)g_output_stream_write_bytes_cb,
                                     g_steal_pointer (&task));
}

static void
valent_mutter_clipboard_selection_write (ValentMutterClipboard *self,
                                         const char            *mimetype,
                                         uint32_t               serial)
{
  g_autoptr (GCancellable) cancellable = NULL;
  g_autoptr (GTask) task = NULL;
  SelectionData *selection = NULL;

  g_assert (VALENT_IS_MUTTER_CLIPBOARD (self));

  selection = g_new0 (SelectionData, 1);
  selection->content = g_bytes_ref (self->content);
  selection->mimetype = g_strdup (mimetype);
  selection->serial = serial;

  cancellable = valent_object_ref_cancellable (VALENT_OBJECT (self));

  task = g_task_new (self, cancellable, selection_transfer_cb, NULL);
  g_task_set_task_data (task, selection, selection_data_free);
  g_task_set_source_tag (task, valent_mutter_clipboard_selection_write);

  g_dbus_connection_call (self->connection,
                          REMOTE_DESKTOP_NAME,
                          self->session_path,
                          REMOTE_DESKTOP_SESSION_IFACE,
                          "SelectionWrite",
                          g_variant_new ("(u)", selection->serial),
                          G_VARIANT_TYPE ("(h)"),
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          cancellable,
                          (GAsyncReadyCallback)selection_write_cb,
                          g_steal_pointer (&task));
}

/*
 * org.gnome.Mutter.RemoteDesktop.Session Callbacks
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
  ValentMutterClipboard *self = VALENT_MUTTER_CLIPBOARD (user_data);

  g_assert (VALENT_IS_MUTTER_CLIPBOARD (self));
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

  g_clear_pointer (&self->session_path, g_free);
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
  ValentMutterClipboard *self = VALENT_MUTTER_CLIPBOARD (user_data);
  g_autoptr (GVariant) options = NULL;
  g_autoptr (GVariant) mimetypes = NULL;

  g_assert (VALENT_IS_CLIPBOARD_ADAPTER (adapter));
  g_assert (g_str_equal (signal_name, "SelectionOwnerChanged"));

  options = g_variant_get_child_value (parameters, 0);

  if (g_variant_lookup (options, "mime-types", "(@as)", &mimetypes))
    {
      g_clear_pointer (&self->mimetypes, g_variant_unref);
      self->mimetypes = g_variant_ref_sink (g_steal_pointer (&mimetypes));
    }

  if (!g_variant_lookup (options, "session-is-owner", "b", &self->is_owner))
    self->is_owner = FALSE;

  /* Free the cache if ownership of the selection has been lost */
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
  ValentMutterClipboard *self = VALENT_MUTTER_CLIPBOARD (user_data);
  const char *mimetype;
  uint32_t serial;

  g_assert (VALENT_IS_MUTTER_CLIPBOARD (self));
  g_assert (g_str_equal (signal_name, "SelectionTransfer"));

  g_variant_get (parameters, "(&su)", &mimetype, &serial);
  valent_mutter_clipboard_selection_write (self, mimetype, serial);
}

/*
 * ValentClipboardAdapter
 */
static GStrv
valent_mutter_clipboard_get_mimetypes (ValentClipboardAdapter *adapter)
{
  ValentMutterClipboard *self = VALENT_MUTTER_CLIPBOARD (adapter);

  g_assert (VALENT_IS_MUTTER_CLIPBOARD (self));

  if (self->mimetypes == NULL)
    return NULL;

  return g_variant_dup_strv (self->mimetypes, NULL);
}

static int64_t
valent_mutter_clipboard_get_timestamp (ValentClipboardAdapter *adapter)
{
  ValentMutterClipboard *self = VALENT_MUTTER_CLIPBOARD (adapter);

  g_assert (VALENT_IS_MUTTER_CLIPBOARD (self));

  return self->timestamp;
}

static void
valent_mutter_clipboard_read_bytes (ValentClipboardAdapter *adapter,
                                    const char             *mimetype,
                                    GCancellable           *cancellable,
                                    GAsyncReadyCallback     callback,
                                    gpointer                user_data)
{
  ValentMutterClipboard *self = VALENT_MUTTER_CLIPBOARD (adapter);
  g_autoptr (GTask) task = NULL;
  g_autofree const char **mimetypes = NULL;

  g_assert (VALENT_IS_MUTTER_CLIPBOARD (self));
  g_assert (mimetype != NULL && *mimetype != '\0');

  if (self->connection == NULL || self->session_path == NULL)
    return g_task_report_new_error (adapter, callback, user_data, callback,
                                    G_IO_ERROR,
                                    G_IO_ERROR_DBUS_ERROR,
                                    "Clipboard service not available.");

  if (self->mimetypes == NULL)
    return g_task_report_new_error (adapter, callback, user_data, callback,
                                    G_IO_ERROR,
                                    G_IO_ERROR_NOT_SUPPORTED,
                                    "Clipboard empty");

  mimetypes = g_variant_get_strv (self->mimetypes, NULL);

  if (!g_strv_contains (mimetypes, mimetype))
    return g_task_report_new_error (adapter, callback, user_data, callback,
                                    G_IO_ERROR,
                                    G_IO_ERROR_NOT_SUPPORTED,
                                    "%s format not available.",
                                    mimetype);

  task = g_task_new (adapter, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_mutter_clipboard_read_bytes);

  if (self->is_owner && self->content != NULL)
    return g_task_return_pointer (task,
                                  g_bytes_ref (self->content),
                                  (GDestroyNotify)g_bytes_unref);

  g_dbus_connection_call (self->connection,
                          REMOTE_DESKTOP_NAME,
                          self->session_path,
                          REMOTE_DESKTOP_SESSION_IFACE,
                          "SelectionRead",
                          g_variant_new ("(s)", mimetype),
                          G_VARIANT_TYPE ("(h)"),
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          cancellable,
                          (GAsyncReadyCallback)selection_read_cb,
                          g_steal_pointer (&task));
}

static void
set_selection_cb (GDBusConnection *connection,
                  GAsyncResult    *result,
                  gpointer         user_data)
{
  g_autoptr (GTask) task = G_TASK (user_data);
  g_autoptr (GVariant) reply = NULL;
  g_autoptr (GError) error = NULL;

  reply = g_dbus_connection_call_finish (connection, result, &error);

  if (reply == NULL)
    {
      g_dbus_error_strip_remote_error (error);
      return g_task_return_error (task, g_steal_pointer (&error));
    }

  g_task_return_boolean (task, TRUE);
}

static void
valent_mutter_clipboard_write_bytes (ValentClipboardAdapter *adapter,
                                     const char             *mimetype,
                                     GBytes                 *bytes,
                                     GCancellable           *cancellable,
                                     GAsyncReadyCallback     callback,
                                     gpointer                user_data)
{
  ValentMutterClipboard *self = VALENT_MUTTER_CLIPBOARD (adapter);
  g_autoptr (GTask) task = NULL;
  GVariantBuilder options;

  g_assert (VALENT_IS_MUTTER_CLIPBOARD (self));
  g_assert (bytes != NULL || (mimetype != NULL && *mimetype != '\0'));

  if (self->connection == NULL || self->session_path == NULL)
    return g_task_report_new_error (adapter, callback, user_data, callback,
                                    G_IO_ERROR,
                                    G_IO_ERROR_DBUS_ERROR,
                                    "Clipboard service not available.");

  /* Update the local content */
  g_clear_pointer (&self->content, g_bytes_unref);
  self->content = g_bytes_ref (bytes);
  g_clear_pointer (&self->mimetypes, g_variant_unref);
  self->mimetypes = g_variant_new_strv (VALENT_STRV_INIT (mimetype), -1);
  g_variant_ref_sink (self->mimetypes);
  self->timestamp = valent_timestamp_ms ();

  /* Inform Mutter */
  task = g_task_new (adapter, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_mutter_clipboard_write_bytes);

  g_variant_builder_init (&options, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add (&options, "{sv}", "mime-types", self->mimetypes);

  g_dbus_connection_call (self->connection,
                          REMOTE_DESKTOP_NAME,
                          self->session_path,
                          REMOTE_DESKTOP_SESSION_IFACE,
                          "SetSelection",
                          g_variant_new ("(@a{sv})",
                                         g_variant_builder_end (&options)),
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          cancellable,
                          (GAsyncReadyCallback)set_selection_cb,
                          g_steal_pointer (&task));
}

/*
 * GAsyncInitable
 */
static void
create_session_error_handler (GObject      *object,
                              GAsyncResult *result,
                              gpointer      user_data)
{
  ValentMutterClipboard *self = VALENT_MUTTER_CLIPBOARD (object);
  g_autoptr (GError) error = NULL;

  if (g_task_propagate_boolean (G_TASK (result), &error) ||
      valent_object_in_destruction (VALENT_OBJECT (self)))
    return;

  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
      valent_extension_plugin_state_changed (VALENT_EXTENSION (self),
                                             VALENT_PLUGIN_STATE_INACTIVE,
                                             NULL);
    }
  else
    {
      valent_extension_plugin_state_changed (VALENT_EXTENSION (self),
                                             VALENT_PLUGIN_STATE_ERROR,
                                             error);
    }
}

static void
enable_clipboard_cb (GDBusConnection *connection,
                     GAsyncResult    *result,
                     gpointer         user_data)
{
  g_autoptr (GTask) task = G_TASK (user_data);
  ValentMutterClipboard *self = g_task_get_source_object (task);
  g_autoptr (GVariant) reply = NULL;
  g_autoptr (GError) error = NULL;

  reply = g_dbus_connection_call_finish (connection, result, &error);

  if (reply == NULL)
    {
      g_dbus_error_strip_remote_error (error);
      return g_task_return_error (task, g_steal_pointer (&error));
    }

  valent_extension_plugin_state_changed (VALENT_EXTENSION (self),
                                         VALENT_PLUGIN_STATE_ACTIVE,
                                         NULL);
  g_task_return_boolean (task, TRUE);
}

static void
create_session_cb (GDBusConnection *connection,
                   GAsyncResult    *result,
                   gpointer         user_data)
{
  g_autoptr (GTask) task = G_TASK (user_data);
  ValentMutterClipboard *self = g_task_get_source_object (task);
  GCancellable *cancellable = g_task_get_cancellable (task);
  g_autoptr (GVariant) reply = NULL;
  g_autoptr (GError) error = NULL;

  reply = g_dbus_connection_call_finish (connection, result, &error);

  if (reply == NULL)
    {
      g_dbus_error_strip_remote_error (error);
      return g_task_return_error (task, g_steal_pointer (&error));
    }

  g_clear_pointer (&self->session_path, g_free);
  g_variant_get (reply, "(o)", &self->session_path);

  self->closed_id =
    g_dbus_connection_signal_subscribe (connection,
                                        NULL,
                                        REMOTE_DESKTOP_SESSION_IFACE,
                                        "Closed",
                                        self->session_path,
                                        NULL,
                                        G_DBUS_SIGNAL_FLAGS_NONE,
                                        on_closed,
                                        self, NULL);

  self->selection_owner_changed_id =
    g_dbus_connection_signal_subscribe (connection,
                                        NULL,
                                        REMOTE_DESKTOP_SESSION_IFACE,
                                        "SelectionOwnerChanged",
                                        self->session_path,
                                        NULL,
                                        G_DBUS_SIGNAL_FLAGS_NONE,
                                        on_selection_owner_changed,
                                        self, NULL);

  self->selection_transfer_id =
    g_dbus_connection_signal_subscribe (connection,
                                        NULL,
                                        REMOTE_DESKTOP_SESSION_IFACE,
                                        "SelectionTransfer",
                                        self->session_path,
                                        NULL,
                                        G_DBUS_SIGNAL_FLAGS_NONE,
                                        on_selection_transfer,
                                        self, NULL);

  g_dbus_connection_call (connection,
                          REMOTE_DESKTOP_NAME,
                          self->session_path,
                          REMOTE_DESKTOP_SESSION_IFACE,
                          "EnableClipboard",
                          g_variant_parse (G_VARIANT_TYPE ("(a{sv})"),
                                           "(@a{sv} {},)",
                                           NULL,
                                           NULL,
                                           &error),
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          cancellable,
                          (GAsyncReadyCallback)enable_clipboard_cb,
                          g_steal_pointer (&task));
}

static void
on_name_appeared (GDBusConnection       *connection,
                  const char            *name,
                  const char            *name_owner,
                  ValentMutterClipboard *self)
{
  g_autoptr (GTask) task = NULL;
  g_autoptr (GCancellable) destroy = NULL;

  g_assert (VALENT_IS_MUTTER_CLIPBOARD (self));

  if (!g_set_object (&self->connection, connection))
    return;

  destroy = valent_object_ref_cancellable (VALENT_OBJECT (self));
  task = g_task_new (self, destroy, create_session_error_handler, NULL);
  g_task_set_source_tag (task, on_name_appeared);

  g_dbus_connection_call (self->connection,
                          REMOTE_DESKTOP_NAME,
                          REMOTE_DESKTOP_PATH,
                          REMOTE_DESKTOP_IFACE,
                          "CreateSession",
                          NULL,
                          G_VARIANT_TYPE ("(o)"),
                          G_DBUS_CALL_FLAGS_NO_AUTO_START,
                          -1,
                          destroy,
                          (GAsyncReadyCallback)create_session_cb,
                          g_steal_pointer (&task));
}

static void
on_name_vanished (GDBusConnection       *connection,
                  const char            *name,
                  ValentMutterClipboard *self)
{
  g_assert (VALENT_IS_MUTTER_CLIPBOARD (self));

  if (self->connection != NULL)
    {
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

      if (self->session_path != NULL)
        {
          g_dbus_connection_call (self->connection,
                                  REMOTE_DESKTOP_NAME,
                                  self->session_path,
                                  REMOTE_DESKTOP_SESSION_IFACE,
                                  "DisableClipboard",
                                  NULL,
                                  NULL,
                                  G_DBUS_CALL_FLAGS_NO_AUTO_START,
                                  -1,
                                  NULL,
                                  NULL,
                                  NULL);

          // HACK: `Start()` must called before `Stop()` will close the session
          g_dbus_connection_call (self->connection,
                                  REMOTE_DESKTOP_NAME,
                                  self->session_path,
                                  REMOTE_DESKTOP_SESSION_IFACE,
                                  "Start",
                                  NULL,
                                  NULL,
                                  G_DBUS_CALL_FLAGS_NO_AUTO_START,
                                  -1,
                                  NULL,
                                  NULL,
                                  NULL);
          g_dbus_connection_call (self->connection,
                                  REMOTE_DESKTOP_NAME,
                                  self->session_path,
                                  REMOTE_DESKTOP_SESSION_IFACE,
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
  g_clear_pointer (&self->session_path, g_free);
  valent_extension_plugin_state_changed (VALENT_EXTENSION (self),
                                         VALENT_PLUGIN_STATE_INACTIVE,
                                         NULL);
}

/*
 * ValentObject
 */
static void
valent_mutter_clipboard_destroy (ValentObject *object)
{
  ValentMutterClipboard *self = VALENT_MUTTER_CLIPBOARD (object);

  g_clear_handle_id (&self->watcher_id, g_bus_unwatch_name);
  on_name_vanished (self->connection, REMOTE_DESKTOP_NAME, self);

  g_clear_pointer (&self->content, g_bytes_unref);
  g_clear_pointer (&self->mimetypes, g_variant_unref);

  VALENT_OBJECT_CLASS (valent_mutter_clipboard_parent_class)->destroy (object);
}

/*
 * GObject
 */
static void
valent_mutter_clipboard_constructed (GObject *object)
{
  ValentMutterClipboard *self = VALENT_MUTTER_CLIPBOARD (object);

  G_OBJECT_CLASS (valent_mutter_clipboard_parent_class)->constructed (object);

  self->watcher_id = g_bus_watch_name (G_BUS_TYPE_SESSION,
                                       REMOTE_DESKTOP_NAME,
                                       G_BUS_NAME_WATCHER_FLAGS_NONE,
                                       (GBusNameAppearedCallback)on_name_appeared,
                                       (GBusNameVanishedCallback)on_name_vanished,
                                       self, NULL);
}

static void
valent_mutter_clipboard_class_init (ValentMutterClipboardClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentObjectClass *vobject_class = VALENT_OBJECT_CLASS (klass);
  ValentClipboardAdapterClass *clipboard_class = VALENT_CLIPBOARD_ADAPTER_CLASS (klass);

  object_class->constructed = valent_mutter_clipboard_constructed;

  vobject_class->destroy = valent_mutter_clipboard_destroy;

  clipboard_class->get_mimetypes = valent_mutter_clipboard_get_mimetypes;
  clipboard_class->get_timestamp = valent_mutter_clipboard_get_timestamp;
  clipboard_class->read_bytes = valent_mutter_clipboard_read_bytes;
  clipboard_class->write_bytes = valent_mutter_clipboard_write_bytes;
}

static void
valent_mutter_clipboard_init (ValentMutterClipboard *self)
{
}

