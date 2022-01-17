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
  PeasExtensionBase  parent_instance;

  ValentDevice      *device;
  GSettings         *settings;

  ValentClipboard   *clipboard;
  gulong             changed_id;

  char              *local_text;
  gint64             local_timestamp;
  char              *remote_text;
  gint64             remote_timestamp;
};

static void valent_device_plugin_iface_init (ValentDevicePluginInterface *iface);

G_DEFINE_TYPE_WITH_CODE (ValentClipboardPlugin, valent_clipboard_plugin, PEAS_TYPE_EXTENSION_BASE,
                         G_IMPLEMENT_INTERFACE (VALENT_TYPE_DEVICE_PLUGIN, valent_device_plugin_iface_init))

enum {
  PROP_0,
  PROP_DEVICE,
  N_PROPERTIES
};

/*
 * Packet Handlers
 */
static void
valent_clipboard_plugin_handle_clipboard (ValentClipboardPlugin *self,
                                          JsonNode              *packet)
{
  JsonObject *body;
  JsonNode *node;
  const char *content;

  g_assert (VALENT_IS_CLIPBOARD_PLUGIN (self));
  g_assert (VALENT_IS_PACKET (packet));

  body = valent_packet_get_body (packet);

  if G_UNLIKELY ((node = json_object_get_member (body, "content")) == NULL ||
                 json_node_get_value_type (node) != G_TYPE_STRING)
    {
      g_debug ("%s: Missing \"content\" field", G_STRFUNC);
      return;
    }

  content = json_node_get_string (node);

  /* Cache remote content */
  g_clear_pointer (&self->remote_text, g_free);
  self->remote_text = g_strdup (content);
  self->remote_timestamp = valent_timestamp_ms ();

  /* Set clipboard */
  if (g_settings_get_boolean (self->settings, "auto-pull"))
    valent_clipboard_set_text (self->clipboard, content);
}

static void
valent_clipboard_plugin_handle_clipboard_connect (ValentClipboardPlugin *self,
                                                  JsonNode              *packet)
{
  JsonObject *body;
  JsonNode *node;
  gint64 timestamp;
  const char *content;

  g_assert (VALENT_IS_CLIPBOARD_PLUGIN (self));
  g_assert (VALENT_IS_PACKET (packet));

  body = valent_packet_get_body (packet);

  if G_UNLIKELY ((node = json_object_get_member (body, "timestamp")) == NULL ||
                 json_node_get_value_type (node) != G_TYPE_INT64)
    {
      g_debug ("%s: missing \"timestamp\" field", G_STRFUNC);
      return;
    }

  timestamp = json_node_get_int (node);

  if G_UNLIKELY ((node = json_object_get_member (body, "content")) == NULL ||
                 json_node_get_value_type (node) != G_TYPE_STRING)
    {
      g_debug ("%s: missing \"content\" field", G_STRFUNC);
      return;
    }

  content = json_node_get_string (node);

  /* Cache remote content */
  g_clear_pointer (&self->remote_text, g_free);
  self->remote_text = g_strdup (content);
  self->remote_timestamp = timestamp;

  /* If the remote content is outdated at connect-time, we won't auto-pull. */
  if (self->remote_timestamp <= self->local_timestamp)
    return;

  /* Set clipboard */
  if (g_settings_get_boolean (self->settings, "auto-pull"))
    valent_clipboard_set_text (self->clipboard, content);
}

/*
 * Packet Providers
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

  valent_device_queue_packet (self->device, packet);
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

  valent_device_queue_packet (self->device, packet);
}

static void
get_text_cb (ValentClipboard       *clipboard,
             GAsyncResult          *result,
             ValentClipboardPlugin *self)
{
  g_autoptr (GError) error = NULL;
  g_autofree char *text = NULL;

  g_assert (VALENT_IS_CLIPBOARD (clipboard));
  g_assert (VALENT_IS_CLIPBOARD_PLUGIN (self));

  text = valent_clipboard_get_text_finish (clipboard, result, &error);

  if (error != NULL)
    {
      if (!valent_error_ignore (error))
        g_warning ("%s(): %s", G_STRFUNC, error->message);

      return;
    }

  if (g_strcmp0 (text, self->local_text) != 0)
    {
      /* Store the local content */
      g_clear_pointer (&self->local_text, g_free);
      self->local_text = g_steal_pointer (&text);
      self->local_timestamp = valent_clipboard_get_timestamp (clipboard);

      /* Inform the device */
      if (g_settings_get_boolean (self->settings, "auto-push"))
        valent_clipboard_plugin_clipboard (self, self->local_text);
    }
}

static void
get_text_connect_cb (ValentClipboard       *clipboard,
                     GAsyncResult          *result,
                     ValentClipboardPlugin *self)
{
  g_autoptr (GError) error = NULL;
  g_autofree char *text = NULL;

  g_assert (VALENT_IS_CLIPBOARD (clipboard));
  g_assert (VALENT_IS_CLIPBOARD_PLUGIN (self));

  text = valent_clipboard_get_text_finish (clipboard, result, &error);

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
  valent_clipboard_plugin_clipboard_connect (self,
                                             self->local_text,
                                             self->local_timestamp);
}

static void
on_clipboard_changed (ValentClipboard       *clipboard,
                      ValentClipboardPlugin *self)
{
  valent_clipboard_get_text_async (clipboard,
                                   NULL,
                                   (GAsyncReadyCallback)get_text_cb,
                                   self);
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
  valent_clipboard_set_text (self->clipboard, self->remote_text);
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
    {"clipboard-pull", clipboard_pull_action, NULL, NULL, NULL},
    {"clipboard-push", clipboard_push_action, NULL, NULL, NULL}
};

/**
 * ValentDevicePlugin
 */
static void
valent_clipboard_plugin_enable (ValentDevicePlugin *plugin)
{
  ValentClipboardPlugin *self = VALENT_CLIPBOARD_PLUGIN (plugin);
  const char *device_id;

  g_assert (VALENT_IS_CLIPBOARD_PLUGIN (self));

  /* Setup GSettings */
  device_id = valent_device_get_id (self->device);
  self->settings = valent_device_plugin_new_settings (device_id, "clipboard");

  /* Register GActions */
  valent_device_plugin_register_actions (plugin,
                                         actions,
                                         G_N_ELEMENTS (actions));

  /* Get the clipboard */
  if (self->clipboard == NULL)
    self->clipboard = valent_clipboard_get_default ();
}

static void
valent_clipboard_plugin_disable (ValentDevicePlugin *plugin)
{
  ValentClipboardPlugin *self = VALENT_CLIPBOARD_PLUGIN (plugin);

  /* Drop clipboard */
  if (self->clipboard != NULL)
    self->clipboard = NULL;

  /* Unregister GActions */
  valent_device_plugin_unregister_actions (plugin,
                                           actions,
                                           G_N_ELEMENTS (actions));

  /* Dispose GSettings */
  g_clear_object (&self->settings);
}

static void
valent_clipboard_plugin_update_state (ValentDevicePlugin *plugin)
{
  ValentClipboardPlugin *self = VALENT_CLIPBOARD_PLUGIN (plugin);
  gboolean connected;
  gboolean paired;
  gboolean available;

  connected = valent_device_get_connected (self->device);
  paired = valent_device_get_paired (self->device);
  available = (connected && paired);

  /* GActions */
  valent_device_plugin_toggle_actions (plugin,
                                       actions, G_N_ELEMENTS (actions),
                                       available);

  /* (Un)watch clipboard changes */
  if (available)
    {
      if (self->changed_id == 0)
        self->changed_id = g_signal_connect (self->clipboard,
                                             "changed",
                                             G_CALLBACK (on_clipboard_changed),
                                             self);

      if (g_settings_get_boolean (self->settings, "auto-push"))
        valent_clipboard_get_text_async (self->clipboard,
                                         NULL,
                                         (GAsyncReadyCallback)get_text_connect_cb,
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
  if (g_strcmp0 (type, "kdeconnect.clipboard") == 0)
    valent_clipboard_plugin_handle_clipboard (self, packet);

  else if (g_strcmp0 (type, "kdeconnect.clipboard.connect") == 0)
    valent_clipboard_plugin_handle_clipboard_connect (self, packet);

  else
    g_assert_not_reached ();
}

static void
valent_device_plugin_iface_init (ValentDevicePluginInterface *iface)
{
  iface->enable = valent_clipboard_plugin_enable;
  iface->disable = valent_clipboard_plugin_disable;
  iface->handle_packet = valent_clipboard_plugin_handle_packet;
  iface->update_state = valent_clipboard_plugin_update_state;
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
  g_clear_object (&self->settings);

  G_OBJECT_CLASS (valent_clipboard_plugin_parent_class)->finalize (object);
}
static void
valent_clipboard_plugin_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  ValentClipboardPlugin *self = VALENT_CLIPBOARD_PLUGIN (object);

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
valent_clipboard_plugin_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  ValentClipboardPlugin *self = VALENT_CLIPBOARD_PLUGIN (object);

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
valent_clipboard_plugin_class_init (ValentClipboardPluginClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = valent_clipboard_plugin_finalize;
  object_class->get_property = valent_clipboard_plugin_get_property;
  object_class->set_property = valent_clipboard_plugin_set_property;

  g_object_class_override_property (object_class, PROP_DEVICE, "device");
}

static void
valent_clipboard_plugin_init (ValentClipboardPlugin *self)
{
}

