// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-ping-plugin"

#include "config.h"

#include <glib/gi18n.h>
#include <gio/gio.h>
#include <json-glib/json-glib.h>
#include <valent.h>

#include "valent-ping-plugin.h"


struct _ValentPingPlugin
{
  ValentDevicePlugin  parent_instance;
};

G_DEFINE_FINAL_TYPE (ValentPingPlugin, valent_ping_plugin, VALENT_TYPE_DEVICE_PLUGIN)


static void
valent_ping_plugin_handle_ping (ValentPingPlugin *self,
                                JsonNode         *packet)
{
  g_autoptr (GNotification) notification = NULL;
  const char *message;
  ValentDevice *device;

  g_assert (VALENT_IS_PING_PLUGIN (self));
  g_assert (VALENT_IS_PACKET (packet));

  /* Check for the optional message */
  if (!valent_packet_get_string (packet, "message", &message))
    message = _("Ping!");

  /* Show a notification */
  device = valent_extension_get_object (VALENT_EXTENSION (self));
  notification = g_notification_new (valent_device_get_name (device));
  g_notification_set_body (notification, message);
  valent_device_plugin_show_notification (VALENT_DEVICE_PLUGIN (self),
                                          "ping",
                                          notification);
}

static void
valent_ping_plugin_send_ping (ValentPingPlugin *self,
                              const char       *message)
{
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonNode) packet = NULL;

  g_assert (VALENT_IS_PING_PLUGIN (self));

  valent_packet_init (&builder, "kdeconnect.ping");

  if (message != NULL && *message != '\0')
    {
      json_builder_set_member_name (builder, "message");
      json_builder_add_string_value (builder, message);
    }

  packet = valent_packet_end (&builder);

  valent_device_plugin_queue_packet (VALENT_DEVICE_PLUGIN (self), packet);
}

/*
 * GActions
 */
static void
ping_action (GSimpleAction *action,
             GVariant      *parameter,
             gpointer       user_data)
{
  ValentPingPlugin *self = VALENT_PING_PLUGIN (user_data);
  const char *message = NULL;

  g_assert (VALENT_IS_PING_PLUGIN (self));

  if (parameter != NULL)
    message = g_variant_get_string (parameter, NULL);

  valent_ping_plugin_send_ping (self, message);
}

static const GActionEntry actions[] = {
    {"ping",    ping_action, NULL, NULL, NULL},
    {"message", ping_action, "s",  NULL, NULL}
};

/*
 * ValentDevicePlugin
 */
static void
valent_ping_plugin_update_state (ValentDevicePlugin *plugin,
                                 ValentDeviceState   state)
{
  gboolean available;

  g_assert (VALENT_IS_PING_PLUGIN (plugin));

  available = (state & VALENT_DEVICE_STATE_CONNECTED) != 0 &&
              (state & VALENT_DEVICE_STATE_PAIRED) != 0;

  valent_extension_toggle_actions (VALENT_EXTENSION (plugin), available);
}

static void
valent_ping_plugin_handle_packet (ValentDevicePlugin *plugin,
                                  const char         *type,
                                  JsonNode           *packet)
{
  ValentPingPlugin *self = VALENT_PING_PLUGIN (plugin);

  g_assert (VALENT_IS_PING_PLUGIN (self));
  g_assert (type != NULL);
  g_assert (VALENT_IS_PACKET (packet));

  if (g_str_equal (type, "kdeconnect.ping"))
    valent_ping_plugin_handle_ping (self, packet);
  else
    g_assert_not_reached ();
}

/*
 * GObject
 */
static void
valent_ping_plugin_constructed (GObject *object)
{
  ValentDevicePlugin *plugin = VALENT_DEVICE_PLUGIN (object);

  g_action_map_add_action_entries (G_ACTION_MAP (plugin),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   plugin);
  valent_device_plugin_set_menu_action (plugin,
                                        "device.ping.ping",
                                        _("Ping"),
                                        "dialog-information-symbolic");

  G_OBJECT_CLASS (valent_ping_plugin_parent_class)->constructed (object);
}

static void
valent_ping_plugin_dispose (GObject *object)
{
  ValentDevicePlugin *plugin = VALENT_DEVICE_PLUGIN (object);

  valent_device_plugin_set_menu_item (plugin, "device.ping.ping", NULL);

  G_OBJECT_CLASS (valent_ping_plugin_parent_class)->dispose (object);
}

static void
valent_ping_plugin_class_init (ValentPingPluginClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentDevicePluginClass *plugin_class = VALENT_DEVICE_PLUGIN_CLASS (klass);

  object_class->constructed = valent_ping_plugin_constructed;
  object_class->dispose = valent_ping_plugin_dispose;

  plugin_class->handle_packet = valent_ping_plugin_handle_packet;
  plugin_class->update_state = valent_ping_plugin_update_state;
}

static void
valent_ping_plugin_init (ValentPingPlugin *self)
{
}

