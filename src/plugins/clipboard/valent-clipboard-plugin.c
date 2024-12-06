// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-clipboard-plugin"

#include "config.h"

#include <gio/gio.h>
#include <json-glib/json-glib.h>
#include <valent.h>

#include "valent-clipboard-plugin.h"


struct _ValentClipboardPlugin
{
  ValentDevicePlugin  parent_instance;

  ValentClipboard    *clipboard;
  unsigned long       changed_id;

  char               *remote_text;
  int64_t             remote_timestamp;
  int64_t             local_timestamp;
  unsigned int        auto_pull : 1;
  unsigned int        auto_push : 1;
};

G_DEFINE_FINAL_TYPE (ValentClipboardPlugin, valent_clipboard_plugin, VALENT_TYPE_DEVICE_PLUGIN)


/*
 * Local Clipboard
 */
static void
valent_clipboard_plugin_clipboard (ValentClipboardPlugin *self,
                                   const char            *content)
{
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonNode) packet = NULL;

  g_return_if_fail (VALENT_IS_CLIPBOARD_PLUGIN (self));

  if (content == NULL)
    return;

  valent_packet_init (&builder, "kdeconnect.clipboard");
  json_builder_set_member_name (builder, "content");
  json_builder_add_string_value (builder, content);
  packet = valent_packet_end (&builder);

  valent_device_plugin_queue_packet (VALENT_DEVICE_PLUGIN (self), packet);
}

static void
valent_clipboard_plugin_clipboard_connect (ValentClipboardPlugin *self,
                                           const char            *content,
                                           int64_t                timestamp)
{
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonNode) packet = NULL;

  g_return_if_fail (VALENT_IS_CLIPBOARD_PLUGIN (self));

  if (content == NULL)
    return;

  valent_packet_init (&builder, "kdeconnect.clipboard.connect");
  json_builder_set_member_name (builder, "content");
  json_builder_add_string_value (builder, content);
  json_builder_set_member_name (builder, "timestamp");
  json_builder_add_int_value (builder, timestamp);
  packet = valent_packet_end (&builder);

  valent_device_plugin_queue_packet (VALENT_DEVICE_PLUGIN (self), packet);
}

static void
valent_clipboard_read_text_cb (ValentClipboard       *clipboard,
                               GAsyncResult          *result,
                               ValentClipboardPlugin *self)
{
  g_autoptr (GError) error = NULL;
  g_autofree char *text = NULL;

  g_assert (VALENT_IS_CLIPBOARD (clipboard));
  g_assert (VALENT_IS_CLIPBOARD_PLUGIN (self));

  text = valent_clipboard_read_text_finish (clipboard, result, &error);

  if (error != NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_debug ("%s(): %s", G_STRFUNC, error->message);

      return;
    }

  /* Skip if the local clipboard is empty, or already synced with the device */
  if (text == NULL || g_strcmp0 (self->remote_text, text) == 0)
    return;

  valent_clipboard_plugin_clipboard (self, text);
}

static void
valent_clipboard_read_text_connect_cb (ValentClipboard       *clipboard,
                                       GAsyncResult          *result,
                                       ValentClipboardPlugin *self)
{
  g_autofree char *text = NULL;
  g_autoptr (GError) error = NULL;

  g_assert (VALENT_IS_CLIPBOARD (clipboard));
  g_assert (VALENT_IS_CLIPBOARD_PLUGIN (self));

  text = valent_clipboard_read_text_finish (clipboard, result, &error);

  if (error != NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s(): %s", G_STRFUNC, error->message);

      return;
    }

  if (text == NULL)
    return;

  valent_clipboard_plugin_clipboard_connect (self, text, self->local_timestamp);
}

static void
valent_clipboard_write_text_cb (ValentClipboard *clipboard,
                                GAsyncResult    *result,
                                gpointer         user_data)
{
  g_autoptr (GError) error = NULL;

  g_assert (VALENT_IS_CLIPBOARD (clipboard));

  if (!valent_clipboard_write_text_finish (clipboard, result, &error) &&
      !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    g_warning ("%s(): %s", G_STRFUNC, error->message);
}

static void
on_auto_pull_changed (GSettings             *settings,
                      const char            *key,
                      ValentClipboardPlugin *self)
{
  ValentDevice *device;
  ValentDeviceState state;
  g_autoptr (GCancellable) destroy = NULL;

  g_assert (G_IS_SETTINGS (settings));
  g_assert (VALENT_IS_CLIPBOARD_PLUGIN (self));

  self->auto_pull = g_settings_get_boolean (settings, "auto-pull");

  if (!self->auto_pull)
    return;

  device = valent_resource_get_source (VALENT_RESOURCE (self));
  state = valent_device_get_state (device);

  if ((state & VALENT_DEVICE_STATE_CONNECTED) != 0 ||
      (state & VALENT_DEVICE_STATE_PAIRED) != 0)
    return;

  destroy = valent_object_ref_cancellable (VALENT_OBJECT (self));
  valent_clipboard_write_text (self->clipboard,
                               self->remote_text,
                               destroy,
                               (GAsyncReadyCallback)valent_clipboard_write_text_cb,
                               NULL);
}

static void
on_auto_push_changed (GSettings             *settings,
                      const char            *key,
                      ValentClipboardPlugin *self)
{
  ValentDevice *device;
  ValentDeviceState state;
  g_autoptr (GCancellable) destroy = NULL;

  g_assert (G_IS_SETTINGS (settings));
  g_assert (VALENT_IS_CLIPBOARD_PLUGIN (self));

  self->auto_push = g_settings_get_boolean (settings, "auto-push");

  if (!self->auto_push)
    return;

  device = valent_resource_get_source (VALENT_RESOURCE (self));
  state = valent_device_get_state (device);

  if ((state & VALENT_DEVICE_STATE_CONNECTED) != 0 ||
      (state & VALENT_DEVICE_STATE_PAIRED) != 0)
    return;

  destroy = valent_object_ref_cancellable (VALENT_OBJECT (self));
  valent_clipboard_read_text (valent_clipboard_get_default (),
                              destroy,
                              (GAsyncReadyCallback)valent_clipboard_read_text_cb,
                              self);

}

static void
on_clipboard_changed (ValentClipboard       *clipboard,
                      ValentClipboardPlugin *self)
{
  g_autoptr (GCancellable) destroy = NULL;

  g_assert (VALENT_IS_CLIPBOARD (clipboard));
  g_assert (VALENT_IS_CLIPBOARD_PLUGIN (self));

  self->local_timestamp = valent_clipboard_get_timestamp (clipboard);

  if (!self->auto_push)
    return;

  destroy = valent_object_ref_cancellable (VALENT_OBJECT (self));
  valent_clipboard_read_text (clipboard,
                              destroy,
                              (GAsyncReadyCallback)valent_clipboard_read_text_cb,
                              self);
}

/*
 * Remote Clipboard
 */
static void
valent_clipboard_plugin_handle_clipboard (ValentClipboardPlugin *self,
                                          JsonNode              *packet)
{
  const char *content;
  g_autoptr (GCancellable) destroy = NULL;

  g_assert (VALENT_IS_CLIPBOARD_PLUGIN (self));
  g_assert (VALENT_IS_PACKET (packet));

  if (!valent_packet_get_string (packet, "content", &content))
    {
      g_debug ("%s(): expected \"content\" field holding a string",
               G_STRFUNC);
      return;
    }

  /* The remote clipboard content is cached, for manual control over syncing,
   * because there is no packet type for requesting it on-demand. */
  g_set_str (&self->remote_text, content);
  self->remote_timestamp = valent_timestamp_ms ();

  if (!self->auto_pull)
    return;

  destroy = valent_object_ref_cancellable (VALENT_OBJECT (self));
  valent_clipboard_write_text (self->clipboard,
                               self->remote_text,
                               destroy,
                               (GAsyncReadyCallback)valent_clipboard_write_text_cb,
                               NULL);
}

static void
valent_clipboard_plugin_handle_clipboard_connect (ValentClipboardPlugin *self,
                                                  JsonNode              *packet)
{
  int64_t timestamp;
  const char *content;
  g_autoptr (GCancellable) destroy = NULL;

  g_assert (VALENT_IS_CLIPBOARD_PLUGIN (self));
  g_assert (VALENT_IS_PACKET (packet));

  if (!valent_packet_get_int (packet, "timestamp", &timestamp))
    {
      g_debug ("%s(): expected \"timestamp\" field holding an integer",
               G_STRFUNC);
      return;
    }

  if (!valent_packet_get_string (packet, "content", &content))
    {
      g_debug ("%s(): expected \"content\" field holding a string",
               G_STRFUNC);
      return;
    }

  /* The remote clipboard content is cached, for manual control over syncing,
   * because there is no packet type for requesting it on-demand. */
  g_clear_pointer (&self->remote_text, g_free);
  self->remote_text = g_strdup (content);
  self->remote_timestamp = timestamp;

  if (self->remote_timestamp <= self->local_timestamp)
    return;

  if (!self->auto_pull)
    return;

  destroy = valent_object_ref_cancellable (VALENT_OBJECT (self));
  valent_clipboard_write_text (self->clipboard,
                               self->remote_text,
                               destroy,
                               (GAsyncReadyCallback)valent_clipboard_write_text_cb,
                               NULL);
}

/*
 * GActions
 */
static void
clipboard_pull_action (GSimpleAction *action,
                       GVariant      *parameter,
                       gpointer       user_data)
{
  ValentClipboardPlugin *self = VALENT_CLIPBOARD_PLUGIN (user_data);
  g_autoptr (GCancellable) destroy = NULL;

  g_assert (VALENT_IS_CLIPBOARD_PLUGIN (self));

  if (self->remote_text == NULL || *self->remote_text == '\0')
    {
      g_debug ("%s(): remote clipboard empty", G_STRFUNC);
      return;
    }

  destroy = valent_object_ref_cancellable (VALENT_OBJECT (self));
  valent_clipboard_write_text (self->clipboard,
                               self->remote_text,
                               destroy,
                               (GAsyncReadyCallback)valent_clipboard_write_text_cb,
                               NULL);
}

static void
clipboard_push_action (GSimpleAction *action,
                       GVariant      *parameter,
                       gpointer       user_data)
{
  ValentClipboardPlugin *self = VALENT_CLIPBOARD_PLUGIN (user_data);
  g_autoptr (GCancellable) destroy = NULL;

  g_assert (VALENT_IS_CLIPBOARD_PLUGIN (self));

  destroy = valent_object_ref_cancellable (VALENT_OBJECT (self));
  valent_clipboard_read_text (valent_clipboard_get_default (),
                              destroy,
                              (GAsyncReadyCallback)valent_clipboard_read_text_cb,
                              self);
}

static const GActionEntry actions[] = {
    {"pull", clipboard_pull_action, NULL, NULL, NULL},
    {"push", clipboard_push_action, NULL, NULL, NULL}
};

/*
 * ValentDevicePlugin
 */
static void
valent_clipboard_plugin_update_state (ValentDevicePlugin *plugin,
                                      ValentDeviceState   state)
{
  ValentClipboardPlugin *self = VALENT_CLIPBOARD_PLUGIN (plugin);
  gboolean available;

  g_assert (VALENT_IS_CLIPBOARD_PLUGIN (self));

  available = (state & VALENT_DEVICE_STATE_CONNECTED) != 0 &&
              (state & VALENT_DEVICE_STATE_PAIRED) != 0;

  valent_extension_toggle_actions (VALENT_EXTENSION (plugin), available);

  if (available)
    {
      if (self->changed_id == 0)
        self->changed_id = g_signal_connect (self->clipboard,
                                             "changed",
                                             G_CALLBACK (on_clipboard_changed),
                                             self);

      if (self->auto_push)
        {
          g_autoptr (GCancellable) destroy = NULL;

          destroy = valent_object_ref_cancellable (VALENT_OBJECT (self));
          valent_clipboard_read_text (self->clipboard,
                                      destroy,
                                      (GAsyncReadyCallback)valent_clipboard_read_text_connect_cb,
                                      self);
        }
    }
  else
    {
      g_clear_signal_handler (&self->changed_id, self->clipboard);
    }
}

static void
valent_clipboard_plugin_handle_packet (ValentDevicePlugin *plugin,
                                       const char         *type,
                                       JsonNode           *packet)
{
  ValentClipboardPlugin *self = VALENT_CLIPBOARD_PLUGIN (plugin);

  g_assert (VALENT_IS_DEVICE_PLUGIN (plugin));
  g_assert (type != NULL);
  g_assert (VALENT_IS_PACKET (packet));

  /* The remote clipboard content changed */
  if (g_str_equal (type, "kdeconnect.clipboard"))
    valent_clipboard_plugin_handle_clipboard (self, packet);

  else if (g_str_equal (type, "kdeconnect.clipboard.connect"))
    valent_clipboard_plugin_handle_clipboard_connect (self, packet);

  else
    g_assert_not_reached ();
}

/*
 * ValentObject
 */
static void
valent_clipboard_plugin_destroy (ValentObject *object)
{
  ValentClipboardPlugin *self = VALENT_CLIPBOARD_PLUGIN (object);

  g_clear_signal_handler (&self->changed_id, self->clipboard);

  VALENT_OBJECT_CLASS (valent_clipboard_plugin_parent_class)->destroy (object);
}

/*
 * GObject
 */
static void
valent_clipboard_plugin_constructed (GObject *object)
{
  ValentClipboardPlugin *self = VALENT_CLIPBOARD_PLUGIN (object);
  ValentDevicePlugin *plugin = VALENT_DEVICE_PLUGIN (object);
  GSettings *settings = NULL;

  g_action_map_add_action_entries (G_ACTION_MAP (plugin),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   plugin);

  settings = valent_extension_get_settings (VALENT_EXTENSION (plugin));
  self->auto_pull = g_settings_get_boolean (settings, "auto-pull");
  g_signal_connect_object (settings,
                           "changed::auto-pull",
                           G_CALLBACK (on_auto_pull_changed),
                           self, 0);

  self->auto_push = g_settings_get_boolean (settings, "auto-push");
  g_signal_connect_object (settings,
                           "changed::auto-push",
                           G_CALLBACK (on_auto_push_changed),
                           self, 0);

  self->clipboard = valent_clipboard_get_default ();
  self->local_timestamp = valent_clipboard_get_timestamp (self->clipboard);

  G_OBJECT_CLASS (valent_clipboard_plugin_parent_class)->constructed (object);
}

static void
valent_clipboard_plugin_finalize (GObject *object)
{
  ValentClipboardPlugin *self = VALENT_CLIPBOARD_PLUGIN (object);

  g_clear_pointer (&self->remote_text, g_free);

  G_OBJECT_CLASS (valent_clipboard_plugin_parent_class)->finalize (object);
}

static void
valent_clipboard_plugin_class_init (ValentClipboardPluginClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentObjectClass *vobject_class = VALENT_OBJECT_CLASS (klass);
  ValentDevicePluginClass *plugin_class = VALENT_DEVICE_PLUGIN_CLASS (klass);

  object_class->constructed = valent_clipboard_plugin_constructed;
  object_class->finalize = valent_clipboard_plugin_finalize;

  vobject_class->destroy = valent_clipboard_plugin_destroy;

  plugin_class->handle_packet = valent_clipboard_plugin_handle_packet;
  plugin_class->update_state = valent_clipboard_plugin_update_state;
}

static void
valent_clipboard_plugin_init (ValentClipboardPlugin *self)
{
}

