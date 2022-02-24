// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-ping-plugin"

#include "config.h"

#include <glib/gi18n.h>
#include <libpeas/peas.h>
#include <libvalent-core.h>

#include "valent-ping-plugin.h"


struct _ValentPingPlugin
{
  ValentDevicePlugin  parent_instance;
};

G_DEFINE_TYPE (ValentPingPlugin, valent_ping_plugin, VALENT_TYPE_DEVICE_PLUGIN)


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
  device = valent_device_plugin_get_device (VALENT_DEVICE_PLUGIN (self));
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
  JsonBuilder *builder;
  g_autoptr (JsonNode) packet = NULL;

  g_assert (VALENT_IS_PING_PLUGIN (self));

  builder = valent_packet_start ("kdeconnect.ping");

  if (message != NULL && *message != '\0')
    {
      json_builder_set_member_name (builder, "message");
      json_builder_add_string_value (builder, message);
    }

  packet = valent_packet_finish (builder);

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

static const ValentMenuEntry items[] = {
    {N_("Ping"), "device.ping.ping", "dialog-information-symbolic"}
};

/**
 * ValentDevicePlugin
 */
static void
valent_ping_plugin_enable (ValentDevicePlugin *plugin)
{
  g_action_map_add_action_entries (G_ACTION_MAP (plugin),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   plugin);
  valent_device_plugin_add_menu_entries (plugin,
                                         items,
                                         G_N_ELEMENTS (items));
}

static void
valent_ping_plugin_disable (ValentDevicePlugin *plugin)
{
  valent_device_plugin_remove_menu_entries (plugin,
                                            items,
                                            G_N_ELEMENTS (items));
}

static void
valent_ping_plugin_update_state (ValentDevicePlugin *plugin,
                                 ValentDeviceState   state)
{
  gboolean available;

  g_assert (VALENT_IS_PING_PLUGIN (plugin));

  available = (state & VALENT_DEVICE_STATE_CONNECTED) != 0 &&
              (state & VALENT_DEVICE_STATE_PAIRED) != 0;

  valent_device_plugin_toggle_actions (plugin, available);
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

  if (g_strcmp0 (type, "kdeconnect.ping") == 0)
    valent_ping_plugin_handle_ping (self, packet);
  else
    g_assert_not_reached ();
}

/*
 * GObject
 */
static void
valent_ping_plugin_class_init (ValentPingPluginClass *klass)
{
  ValentDevicePluginClass *plugin_class = VALENT_DEVICE_PLUGIN_CLASS (klass);

  plugin_class->enable = valent_ping_plugin_enable;
  plugin_class->disable = valent_ping_plugin_disable;
  plugin_class->handle_packet = valent_ping_plugin_handle_packet;
  plugin_class->update_state = valent_ping_plugin_update_state;
}

static void
valent_ping_plugin_init (ValentPingPlugin *self)
{
}

