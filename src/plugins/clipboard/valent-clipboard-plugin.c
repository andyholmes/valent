// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-clipboard-plugin"

#include "config.h"

#include <libpeas/peas.h>
#include <libvalent-core.h>
#include <libvalent-clipboard.h>

#include "valent-clipboard-plugin.h"


struct _ValentClipboardPlugin
{
  ValentDevicePlugin  parent_instance;

  ValentClipboard    *clipboard;
  gulong              changed_id;

  char               *local_text;
  gint64              local_timestamp;
  char               *remote_text;
  gint64              remote_timestamp;
};

G_DEFINE_TYPE (ValentClipboardPlugin, valent_clipboard_plugin, VALENT_TYPE_DEVICE_PLUGIN)


/*
 * Local Clipboard
 */
static void
valent_clipboard_plugin_clipboard (ValentClipboardPlugin *self,
                                   const char            *content)
{
  JsonBuilder *builder;
  g_autoptr (JsonNode) packet = NULL;

  g_return_if_fail (VALENT_IS_CLIPBOARD_PLUGIN (self));

  /* Refuse to send %NULL */
  if (content == NULL)
    return;

  /* Build the packet */
  builder = valent_packet_start ("kdeconnect.clipboard");
  json_builder_set_member_name (builder, "content");
  json_builder_add_string_value (builder, content);
  packet = valent_packet_finish (builder);

  valent_device_plugin_queue_packet (VALENT_DEVICE_PLUGIN (self), packet);
}

static void
valent_clipboard_plugin_clipboard_connect (ValentClipboardPlugin *self,
                                           const char            *content,
                                           gint64                 timestamp)
{
  JsonBuilder *builder;
  g_autoptr (JsonNode) packet = NULL;

  g_return_if_fail (VALENT_IS_CLIPBOARD_PLUGIN (self));

  /* Refuse to send %NULL */
  if (self->local_text == NULL)
    return;

  /* Build the packet */
  builder = valent_packet_start ("kdeconnect.clipboard.connect");
  json_builder_set_member_name (builder, "content");
  json_builder_add_string_value (builder, content);
  json_builder_set_member_name (builder, "timestamp");
  json_builder_add_int_value (builder, timestamp);
  packet = valent_packet_finish (builder);

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
      if (!valent_error_ignore (error))
        g_warning ("%s(): %s", G_STRFUNC, error->message);

      return;
    }

  if (g_strcmp0 (text, self->local_text) != 0)
    {
      GSettings *settings;

      /* Store the local content */
      g_clear_pointer (&self->local_text, g_free);
      self->local_text = g_steal_pointer (&text);
      self->local_timestamp = valent_clipboard_get_timestamp (clipboard);

      /* Inform the device */
      settings = valent_device_plugin_get_settings (VALENT_DEVICE_PLUGIN (self));

      if (g_settings_get_boolean (settings, "auto-push"))
        valent_clipboard_plugin_clipboard (self, self->local_text);
    }
}

static void
valent_clipboard_read_text_connect_cb (ValentClipboard       *clipboard,
                                       GAsyncResult          *result,
                                       ValentClipboardPlugin *self)
{
  g_autoptr (GError) error = NULL;
  g_autofree char *text = NULL;
  GSettings *settings;

  g_assert (VALENT_IS_CLIPBOARD (clipboard));
  g_assert (VALENT_IS_CLIPBOARD_PLUGIN (self));

  text = valent_clipboard_read_text_finish (clipboard, result, &error);

  if (error != NULL)
    {
      if (!valent_error_ignore (error))
        g_warning ("%s(): %s", G_STRFUNC, error->message);

      return;
    }

  /* Store the local content */
  if (g_strcmp0 (text, self->local_text) != 0)
    {
      g_clear_pointer (&self->local_text, g_free);
      self->local_text = g_steal_pointer (&text);
      self->local_timestamp = valent_clipboard_get_timestamp (clipboard);
    }

  /* Inform the device */
  settings = valent_device_plugin_get_settings (VALENT_DEVICE_PLUGIN (self));

  if (g_settings_get_boolean (settings, "auto-push"))
    valent_clipboard_plugin_clipboard_connect (self,
                                               self->local_text,
                                               self->local_timestamp);
}

static void
on_clipboard_changed (ValentClipboard       *clipboard,
                      ValentClipboardPlugin *self)
{
  valent_clipboard_read_text (clipboard,
                              NULL,
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
  GSettings *settings;
  const char *content;

  g_assert (VALENT_IS_CLIPBOARD_PLUGIN (self));
  g_assert (VALENT_IS_PACKET (packet));

  if (!valent_packet_get_string (packet, "content", &content))
    {
      g_warning ("%s(): expected \"content\" field holding a string",
                 G_STRFUNC);
      return;
    }

  /* Cache remote content */
  g_clear_pointer (&self->remote_text, g_free);
  self->remote_text = g_strdup (content);
  self->remote_timestamp = valent_timestamp_ms ();

  /* Set clipboard */
  settings = valent_device_plugin_get_settings (VALENT_DEVICE_PLUGIN (self));

  if (g_settings_get_boolean (settings, "auto-pull"))
    valent_clipboard_write_text (self->clipboard, content, NULL, NULL, NULL);
}

static void
valent_clipboard_plugin_handle_clipboard_connect (ValentClipboardPlugin *self,
                                                  JsonNode              *packet)
{
  GSettings *settings;
  gint64 timestamp;
  const char *content;

  g_assert (VALENT_IS_CLIPBOARD_PLUGIN (self));
  g_assert (VALENT_IS_PACKET (packet));

  if (!valent_packet_get_int (packet, "timestamp", &timestamp))
    {
      g_warning ("%s(): expected \"timestamp\" field holding an integer",
                 G_STRFUNC);
      return;
    }

  if (!valent_packet_get_string (packet, "content", &content))
    {
      g_warning ("%s(): expected \"content\" field holding a string",
                 G_STRFUNC);
      return;
    }

  /* Cache remote content */
  g_clear_pointer (&self->remote_text, g_free);
  self->remote_text = g_strdup (content);
  self->remote_timestamp = timestamp;

  /* If the remote content is outdated at connect-time, we won't auto-pull. */
  if (self->remote_timestamp <= self->local_timestamp)
    return;

  /* Set clipboard */
  settings = valent_device_plugin_get_settings (VALENT_DEVICE_PLUGIN (self));

  if (g_settings_get_boolean (settings, "auto-pull"))
    valent_clipboard_write_text (self->clipboard, content, NULL, NULL, NULL);
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

  g_assert (VALENT_IS_CLIPBOARD_PLUGIN (self));

  /* Set the local clipboard text from the remote buffer */
  valent_clipboard_write_text (self->clipboard,
                               self->remote_text,
                               NULL,
                               NULL,
                               NULL);
}

static void
clipboard_push_action (GSimpleAction *action,
                       GVariant      *parameter,
                       gpointer       user_data)
{
  ValentClipboardPlugin *self = VALENT_CLIPBOARD_PLUGIN (user_data);

  g_assert (VALENT_IS_CLIPBOARD_PLUGIN (self));

  /* Send the local buffer to the remote device */
  valent_clipboard_plugin_clipboard (self, self->local_text);
}

static const GActionEntry actions[] = {
    {"pull", clipboard_pull_action, NULL, NULL, NULL},
    {"push", clipboard_push_action, NULL, NULL, NULL}
};

/*
 * ValentDevicePlugin
 */
static void
valent_clipboard_plugin_enable (ValentDevicePlugin *plugin)
{
  ValentClipboardPlugin *self = VALENT_CLIPBOARD_PLUGIN (plugin);

  g_assert (VALENT_IS_CLIPBOARD_PLUGIN (self));

  g_action_map_add_action_entries (G_ACTION_MAP (plugin),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   plugin);

  /* Ensure the ValentClipboard component is loaded */
  if (self->clipboard == NULL)
    self->clipboard = valent_clipboard_get_default ();
}

static void
valent_clipboard_plugin_disable (ValentDevicePlugin *plugin)
{
  ValentClipboardPlugin *self = VALENT_CLIPBOARD_PLUGIN (plugin);

  /* We're about to be disposed, so stop watching the clipboard for changes */
  g_clear_signal_handler (&self->changed_id, self->clipboard);
}

static void
valent_clipboard_plugin_update_state (ValentDevicePlugin *plugin,
                                      ValentDeviceState   state)
{
  ValentClipboardPlugin *self = VALENT_CLIPBOARD_PLUGIN (plugin);
  gboolean available;

  g_assert (VALENT_IS_CLIPBOARD_PLUGIN (self));

  available = (state & VALENT_DEVICE_STATE_CONNECTED) != 0 &&
              (state & VALENT_DEVICE_STATE_PAIRED) != 0;

  valent_device_plugin_toggle_actions (plugin, available);

  if (available)
    {
      if (self->changed_id == 0)
        self->changed_id = g_signal_connect (self->clipboard,
                                             "changed",
                                             G_CALLBACK (on_clipboard_changed),
                                             self);

      valent_clipboard_read_text (self->clipboard,
                                  NULL,
                                  (GAsyncReadyCallback)valent_clipboard_read_text_connect_cb,
                                  self);
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
  if (strcmp (type, "kdeconnect.clipboard") == 0)
    valent_clipboard_plugin_handle_clipboard (self, packet);

  else if (strcmp (type, "kdeconnect.clipboard.connect") == 0)
    valent_clipboard_plugin_handle_clipboard_connect (self, packet);

  else
    g_assert_not_reached ();
}

/*
 * GObject
 */
static void
valent_clipboard_plugin_finalize (GObject *object)
{
  ValentClipboardPlugin *self = VALENT_CLIPBOARD_PLUGIN (object);

  g_clear_pointer (&self->local_text, g_free);
  g_clear_pointer (&self->remote_text, g_free);

  G_OBJECT_CLASS (valent_clipboard_plugin_parent_class)->finalize (object);
}

static void
valent_clipboard_plugin_class_init (ValentClipboardPluginClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentDevicePluginClass *plugin_class = VALENT_DEVICE_PLUGIN_CLASS (klass);

  object_class->finalize = valent_clipboard_plugin_finalize;

  plugin_class->enable = valent_clipboard_plugin_enable;
  plugin_class->disable = valent_clipboard_plugin_disable;
  plugin_class->handle_packet = valent_clipboard_plugin_handle_packet;
  plugin_class->update_state = valent_clipboard_plugin_update_state;
}

static void
valent_clipboard_plugin_init (ValentClipboardPlugin *self)
{
}

