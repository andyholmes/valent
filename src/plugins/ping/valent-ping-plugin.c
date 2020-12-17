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
  PeasExtensionBase  parent_instance;

  ValentDevice      *device;
};

static void valent_device_plugin_iface_init (ValentDevicePluginInterface *iface);

G_DEFINE_TYPE_WITH_CODE (ValentPingPlugin, valent_ping_plugin, PEAS_TYPE_EXTENSION_BASE,
                         G_IMPLEMENT_INTERFACE (VALENT_TYPE_DEVICE_PLUGIN, valent_device_plugin_iface_init))

enum {
  PROP_0,
  PROP_DEVICE,
  N_PROPERTIES
};


static void
valent_ping_plugin_handle_ping (ValentPingPlugin *self,
                                JsonNode         *packet)
{
  g_autoptr (GNotification) notification = NULL;
  JsonObject *body;
  const char *message;

  g_assert (VALENT_IS_PING_PLUGIN (self));
  g_assert (VALENT_IS_PACKET (packet));

  /* Check for the optional message */
  body = valent_packet_get_body (packet);
  message = json_object_get_string_member_with_default (body, "message",
                                                        _("Ping!"));

  /* Show a notification */
  notification = g_notification_new (valent_device_get_name (self->device));
  g_notification_set_body (notification, message);
  valent_device_show_notification (self->device, "ping", notification);
}

static void
valent_ping_plugin_send_ping (ValentPingPlugin *self,
                              const char       *message)
{
  JsonBuilder *builder;
  g_autoptr (JsonNode) packet = NULL;

  g_assert (VALENT_IS_PING_PLUGIN (self));

  builder = valent_packet_start ("kdeconnect.ping");

  if (message != NULL)
    {
      json_builder_set_member_name (builder, "message");
      json_builder_add_string_value (builder, message);
    }

  packet = valent_packet_finish (builder);

  valent_device_queue_packet (self->device, packet);
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
    {"ping",         ping_action, NULL, NULL, NULL},
    {"ping-message", ping_action, "s",  NULL, NULL}
};

static const ValentMenuEntry items[] = {
    {N_("Ping"), "device.ping", "dialog-information-symbolic"}
};

/**
 * ValentDevicePlugin
 */
static void
valent_ping_plugin_enable (ValentDevicePlugin *plugin)
{
  ValentPingPlugin *self = VALENT_PING_PLUGIN (plugin);

  g_assert (VALENT_IS_PING_PLUGIN (self));

  /* Register GActions */
  valent_device_plugin_register_actions (plugin,
                                         actions,
                                         G_N_ELEMENTS (actions));

  /* Register GMenu items */
  valent_device_plugin_add_menu_entries (plugin,
                                         items,
                                         G_N_ELEMENTS (items));
}

static void
valent_ping_plugin_disable (ValentDevicePlugin *plugin)
{
  /* Unregister GMenu items */
  valent_device_plugin_remove_menu_entries (plugin,
                                            items,
                                            G_N_ELEMENTS (items));

  /* Unregister GActions */
  valent_device_plugin_unregister_actions (plugin,
                                           actions,
                                           G_N_ELEMENTS (actions));
}

static void
valent_ping_plugin_update_state (ValentDevicePlugin *plugin)
{
  ValentPingPlugin *self = VALENT_PING_PLUGIN (plugin);
  gboolean connected;
  gboolean paired;
  gboolean available;

  g_assert (VALENT_IS_PING_PLUGIN (self));

  connected = valent_device_get_connected (self->device);
  paired = valent_device_get_paired (self->device);
  available = (connected && paired);

  /* GActions */
  valent_device_plugin_toggle_actions (plugin,
                                       actions, G_N_ELEMENTS (actions),
                                       available);
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

static void
valent_device_plugin_iface_init (ValentDevicePluginInterface *iface)
{
  iface->enable = valent_ping_plugin_enable;
  iface->disable = valent_ping_plugin_disable;
  iface->handle_packet = valent_ping_plugin_handle_packet;
  iface->update_state = valent_ping_plugin_update_state;
}

/*
 * GObject
 */
static void
valent_ping_plugin_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  ValentPingPlugin *self = VALENT_PING_PLUGIN (object);

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
valent_ping_plugin_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  ValentPingPlugin *self = VALENT_PING_PLUGIN (object);

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
valent_ping_plugin_class_init (ValentPingPluginClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = valent_ping_plugin_get_property;
  object_class->set_property = valent_ping_plugin_set_property;

  g_object_class_override_property (object_class, PROP_DEVICE, "device");
}

static void
valent_ping_plugin_init (ValentPingPlugin *self)
{
}

