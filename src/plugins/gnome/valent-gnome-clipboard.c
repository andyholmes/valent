// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-gnome-clipboard"

#include "config.h"

#include <gio/gio.h>
#include <gio/gunixinputstream.h>
#include <gio/gunixoutputstream.h>
#include <valent.h>

#include "valent-gnome-clipboard.h"

#define CLIPBOARD_NAME "org.gnome.Shell"
#define CLIPBOARD_PATH "/org/gnome/Shell/Extensions/Valent/Clipboard"
#define CLIPBOARD_IFACE "org.gnome.Shell.Extensions.Valent.Clipboard"


struct _ValentGnomeClipboard
{
  ValentClipboardAdapter  parent_instance;

  GDBusProxy             *proxy;

  GVariant               *mimetypes;
  int64_t                 timestamp;
};

static void   g_async_initable_iface_init (GAsyncInitableIface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (ValentGnomeClipboard, valent_gnome_clipboard, VALENT_TYPE_CLIPBOARD_ADAPTER,
                               G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE, g_async_initable_iface_init));


/*
 * org.gnome.Mutter.RemoteDesktop.Session Callbacks
 */
static void
on_g_signal (GDBusProxy           *proxy,
             const char           *sender_name,
             const char           *signal_name,
             GVariant             *parameters,
             ValentGnomeClipboard *self)
{
  g_autoptr (GVariant) metadata = NULL;
  g_autoptr (GVariant) mimetypes = NULL;

  g_assert (VALENT_IS_GNOME_CLIPBOARD (self));
  g_assert (sender_name != NULL && *sender_name != '\0');

  if (!g_str_equal (signal_name, "Changed"))
    return;

  g_clear_pointer (&self->mimetypes, g_variant_unref);
  metadata = g_variant_get_child_value (parameters, 0);

  if (g_variant_lookup (metadata, "mimetypes", "@as", &mimetypes))
    self->mimetypes = g_variant_ref_sink (g_steal_pointer (&mimetypes));

  if (!g_variant_lookup (metadata, "timestamp", "x", &self->timestamp))
    self->timestamp = valent_timestamp_ms ();

  valent_clipboard_adapter_changed (VALENT_CLIPBOARD_ADAPTER (self));
}

static void
g_dbus_proxy_get_bytes_cb (GDBusProxy   *proxy,
                           GAsyncResult *result,
                           gpointer      user_data)
{
  g_autoptr (GTask) task = G_TASK (user_data);
  g_autoptr (GVariant) reply = NULL;
  g_autoptr (GVariant) contents = NULL;
  const uint8_t *data = NULL;
  size_t data_len = 0;
  g_autoptr (GError) error = NULL;

  if ((reply = g_dbus_proxy_call_finish (proxy, result, &error)) == NULL)
    {
      g_dbus_error_strip_remote_error (error);
      return g_task_return_error (task, g_steal_pointer (&error));
    }

  contents = g_variant_get_child_value (reply, 0);
  data = g_variant_get_fixed_array (contents, &data_len, sizeof (uint8_t));

  g_task_return_pointer (task,
                         g_bytes_new (data, data_len),
                         (GDestroyNotify)g_bytes_unref);
}

static void
g_dbus_proxy_set_bytes_cb (GDBusProxy   *proxy,
                           GAsyncResult *result,
                           gpointer      user_data)
{
  g_autoptr (GTask) task = G_TASK (user_data);
  g_autoptr (GVariant) reply = NULL;
  g_autoptr (GError) error = NULL;

  if ((reply = g_dbus_proxy_call_finish (proxy, result, &error)) == NULL)
    {
      g_dbus_error_strip_remote_error (error);
      return g_task_return_error (task, g_steal_pointer (&error));
    }

  g_task_return_boolean (task, TRUE);
}

/*
 * ValentClipboardAdapter
 */
static GStrv
valent_gnome_clipboard_get_mimetypes (ValentClipboardAdapter *adapter)
{
  ValentGnomeClipboard *self = VALENT_GNOME_CLIPBOARD (adapter);

  g_assert (VALENT_IS_GNOME_CLIPBOARD (self));

  if (self->mimetypes == NULL)
    return NULL;

  return g_variant_dup_strv (self->mimetypes, NULL);
}

static int64_t
valent_gnome_clipboard_get_timestamp (ValentClipboardAdapter *adapter)
{
  ValentGnomeClipboard *self = VALENT_GNOME_CLIPBOARD (adapter);

  g_assert (VALENT_IS_GNOME_CLIPBOARD (self));

  return self->timestamp;
}

static void
valent_gnome_clipboard_read_bytes (ValentClipboardAdapter *adapter,
                                   const char             *mimetype,
                                   GCancellable           *cancellable,
                                   GAsyncReadyCallback     callback,
                                   gpointer                user_data)
{
  ValentGnomeClipboard *self = VALENT_GNOME_CLIPBOARD (adapter);
  g_autoptr (GTask) task = NULL;
  g_autofree const char **mimetypes = NULL;

  g_assert (VALENT_IS_GNOME_CLIPBOARD (self));
  g_assert (mimetype != NULL && *mimetype != '\0');

  if (self->mimetypes != NULL)
    mimetypes = g_variant_get_strv (self->mimetypes, NULL);

  if (mimetypes != NULL && !g_strv_contains (mimetypes, mimetype))
    return g_task_report_new_error (adapter, callback, user_data, callback,
                                    G_IO_ERROR,
                                    G_IO_ERROR_NOT_SUPPORTED,
                                    "%s format not available.",
                                    mimetype);

  task = g_task_new (adapter, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_gnome_clipboard_read_bytes);

  g_dbus_proxy_call (self->proxy,
                     "GetBytes",
                     g_variant_new ("(s)", mimetype),
                     G_DBUS_CALL_FLAGS_NO_AUTO_START,
                     -1,
                     cancellable,
                     (GAsyncReadyCallback)g_dbus_proxy_get_bytes_cb,
                     g_steal_pointer (&task));
}

static void
valent_gnome_clipboard_write_bytes (ValentClipboardAdapter *adapter,
                                    const char             *mimetype,
                                    GBytes                 *bytes,
                                    GCancellable           *cancellable,
                                    GAsyncReadyCallback     callback,
                                    gpointer                user_data)
{
  ValentGnomeClipboard *self = VALENT_GNOME_CLIPBOARD (adapter);
  g_autoptr (GTask) task = NULL;
  GVariant *content;

  g_assert (VALENT_IS_GNOME_CLIPBOARD (self));
  g_assert (bytes != NULL || (mimetype != NULL && *mimetype != '\0'));

  task = g_task_new (adapter, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_gnome_clipboard_write_bytes);

  content = g_variant_new_fixed_array (G_VARIANT_TYPE_BYTE,
                                       g_bytes_get_data (bytes, NULL),
                                       g_bytes_get_size (bytes),
                                       sizeof (uint8_t));
  g_dbus_proxy_call (self->proxy,
                     "SetBytes",
                     g_variant_new ("(s@ay)", mimetype, content),
                     G_DBUS_CALL_FLAGS_NO_AUTO_START,
                     -1,
                     cancellable,
                     (GAsyncReadyCallback)g_dbus_proxy_set_bytes_cb,
                     g_steal_pointer (&task));
}

/*
 * GAsyncInitable
 */
static void
on_name_owner_changed (GDBusProxy           *proxy,
                       GParamSpec           *pspec,
                       ValentGnomeClipboard *self)
{
  g_autofree char *name_owner = NULL;

  g_assert (VALENT_IS_GNOME_CLIPBOARD (self));

  if ((name_owner = g_dbus_proxy_get_name_owner (proxy)) != NULL)
    {
      valent_extension_plugin_state_changed (VALENT_EXTENSION (self),
                                             VALENT_PLUGIN_STATE_ACTIVE,
                                             NULL);
    }
  else
    {
      valent_extension_plugin_state_changed (VALENT_EXTENSION (self),
                                             VALENT_PLUGIN_STATE_INACTIVE,
                                             NULL);
    }
}

static void
g_dbus_proxy_new_for_bus_cb (GDBusProxy   *proxy,
                             GAsyncResult *result,
                             gpointer      user_data)
{
  g_autoptr (GTask) task = G_TASK (user_data);
  ValentGnomeClipboard *self = g_task_get_source_object (task);
  g_autoptr (GError) error = NULL;

  g_assert (G_IS_TASK (task));

  if ((self->proxy = g_dbus_proxy_new_for_bus_finish (result, &error)) == NULL)
    {
      valent_extension_plugin_state_changed (VALENT_EXTENSION (self),
                                             VALENT_PLUGIN_STATE_ERROR,
                                             error);
      return g_task_return_error (task, g_steal_pointer (&error));
    }

  g_signal_connect_object (self->proxy,
                           "g-signal",
                           G_CALLBACK (on_g_signal),
                           self, 0);
  g_signal_connect_object (self->proxy,
                           "notify::g-name-owner",
                           G_CALLBACK (on_name_owner_changed),
                           self, 0);
  on_name_owner_changed (self->proxy, NULL, self);

  g_task_return_boolean (task, TRUE);
}

static void
valent_gnome_clipboard_init_async (GAsyncInitable      *initable,
                                   int                  io_priority,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data)
{
  g_autoptr (GTask) task = NULL;
  g_autoptr (GCancellable) destroy = NULL;

  g_assert (VALENT_IS_GNOME_CLIPBOARD (initable));

  /* Cede the primary position until complete */
  valent_extension_plugin_state_changed (VALENT_EXTENSION (initable),
                                         VALENT_PLUGIN_STATE_INACTIVE,
                                         NULL);

  /* Cancel initialization if the object is destroyed */
  destroy = valent_object_attach_cancellable (VALENT_OBJECT (initable),
                                              cancellable);

  task = g_task_new (initable, destroy, callback, user_data);
  g_task_set_priority (task, io_priority);
  g_task_set_source_tag (task, valent_gnome_clipboard_init_async);

  g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                            G_DBUS_PROXY_FLAGS_NONE,
                            NULL,
                            CLIPBOARD_NAME,
                            CLIPBOARD_PATH,
                            CLIPBOARD_IFACE,
                            destroy,
                            (GAsyncReadyCallback)g_dbus_proxy_new_for_bus_cb,
                            g_steal_pointer (&task));
}

static void
g_async_initable_iface_init (GAsyncInitableIface *iface)
{
  iface->init_async = valent_gnome_clipboard_init_async;
}


/*
 * GObject
 */
static void
valent_gnome_clipboard_dispose (GObject *object)
{
  ValentGnomeClipboard *self = VALENT_GNOME_CLIPBOARD (object);

  if (self->proxy != NULL)
    {
      g_signal_handlers_disconnect_by_func (self->proxy, self, on_g_signal);
      g_signal_handlers_disconnect_by_func (self->proxy, self, on_name_owner_changed);
      g_clear_object (&self->proxy);
    }

  g_clear_pointer (&self->mimetypes, g_variant_unref);

  G_OBJECT_CLASS (valent_gnome_clipboard_parent_class)->dispose (object);
}

static void
valent_gnome_clipboard_class_init (ValentGnomeClipboardClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentClipboardAdapterClass *clipboard_class = VALENT_CLIPBOARD_ADAPTER_CLASS (klass);

  object_class->dispose = valent_gnome_clipboard_dispose;

  clipboard_class->get_mimetypes = valent_gnome_clipboard_get_mimetypes;
  clipboard_class->get_timestamp = valent_gnome_clipboard_get_timestamp;
  clipboard_class->read_bytes = valent_gnome_clipboard_read_bytes;
  clipboard_class->write_bytes = valent_gnome_clipboard_write_bytes;
}

static void
valent_gnome_clipboard_init (ValentGnomeClipboard *self)
{
}

