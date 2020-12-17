// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-lock-plugin"

#include "config.h"

#include <glib/gi18n.h>
#include <libpeas/peas.h>
#include <libvalent-core.h>
#include <libvalent-session.h>

#include "valent-lock-plugin.h"


struct _ValentLockPlugin
{
  PeasExtensionBase  parent_instance;

  ValentDevice      *device;
  ValentSession     *session;

  unsigned int       remote_locked : 1;
  unsigned int       local_locked : 1;
};

static void valent_device_plugin_iface_init (ValentDevicePluginInterface *iface);

G_DEFINE_TYPE_WITH_CODE (ValentLockPlugin, valent_lock_plugin, PEAS_TYPE_EXTENSION_BASE,
                         G_IMPLEMENT_INTERFACE (VALENT_TYPE_DEVICE_PLUGIN, valent_device_plugin_iface_init))

static void valent_lock_plugin_request_state (ValentLockPlugin *self);
static void valent_lock_plugin_send_state    (ValentLockPlugin *self);

enum {
  PROP_0,
  PROP_DEVICE,
  N_PROPERTIES
};


static void
update_actions (ValentLockPlugin *self)
{
  GActionGroup *actions;
  GAction *action;

  actions = valent_device_get_actions (self->device);

  action = g_action_map_lookup_action (G_ACTION_MAP (actions), "lock");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), !self->remote_locked);

  action = g_action_map_lookup_action (G_ACTION_MAP (actions), "unlock");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), self->remote_locked);
}


static void
valent_lock_plugin_handle_lock (ValentLockPlugin *self,
                                JsonNode         *packet)
{
  JsonObject *body;

  g_assert (VALENT_IS_LOCK_PLUGIN (self));
  g_assert (VALENT_IS_PACKET (packet));

  /* Check for the optional message */
  body = valent_packet_get_body (packet);

  if (json_object_has_member (body, "isLocked"))
    self->remote_locked = valent_packet_check_boolean (body, "isLocked");

  update_actions (self);
}

static void
valent_lock_plugin_handle_lock_request (ValentLockPlugin *self,
                                        JsonNode         *packet)
{
  JsonObject *body;
  gboolean state;

  g_assert (VALENT_IS_LOCK_PLUGIN (self));
  g_assert (VALENT_IS_PACKET (packet));

  body = valent_packet_get_body (packet);

  if (valent_packet_check_boolean (body, "requestLocked"))
    valent_lock_plugin_send_state (self);

  if (json_object_has_member (body, "setLocked"))
    {
      state = valent_packet_check_boolean (body, "setLocked");
      valent_session_set_locked (self->session, state);
    }
}

static void
valent_lock_plugin_request_state (ValentLockPlugin *self)
{
  JsonBuilder *builder;
  g_autoptr (JsonNode) packet = NULL;

  builder = valent_packet_start ("kdeconnect.lock.request");
  json_builder_set_member_name (builder, "requestLocked");
  json_builder_add_boolean_value (builder, TRUE);
  packet = valent_packet_finish (builder);

  valent_device_queue_packet (self->device, packet);
}

static void
valent_lock_plugin_set_state (ValentLockPlugin *self,
                              gboolean          state)
{
  JsonBuilder *builder;
  g_autoptr (JsonNode) packet = NULL;

  builder = valent_packet_start ("kdeconnect.lock.request");
  json_builder_set_member_name (builder, "setLocked");
  json_builder_add_boolean_value (builder, state);
  packet = valent_packet_finish (builder);

  valent_device_queue_packet (self->device, packet);
}

static void
valent_lock_plugin_send_state (ValentLockPlugin *self)
{
  JsonBuilder *builder;
  g_autoptr (JsonNode) packet = NULL;
  gboolean state;

  g_assert (VALENT_IS_LOCK_PLUGIN (self));

  state = valent_session_get_locked (self->session);

  builder = valent_packet_start ("kdeconnect.lock");
  json_builder_set_member_name (builder, "isLocked");
  json_builder_add_boolean_value (builder, state);
  packet = valent_packet_finish (builder);

  valent_device_queue_packet (self->device, packet);
}


/*
 * GActions
 */
static void
lock_action (GSimpleAction *action,
             GVariant      *parameter,
             gpointer       user_data)
{
  ValentLockPlugin *self = VALENT_LOCK_PLUGIN (user_data);

  g_assert (VALENT_IS_LOCK_PLUGIN (self));

  valent_lock_plugin_set_state (self, TRUE);
}

static void
unlock_action (GSimpleAction *action,
               GVariant      *parameter,
               gpointer       user_data)
{
  ValentLockPlugin *self = VALENT_LOCK_PLUGIN (user_data);

  g_assert (VALENT_IS_LOCK_PLUGIN (self));

  valent_lock_plugin_set_state (self, FALSE);
}

static const GActionEntry actions[] = {
    {"lock",       lock_action,   NULL, NULL,    NULL},
    {"unlock",     unlock_action, NULL, NULL,    NULL},
    {"lock-state", NULL,          NULL, "false", NULL},
};

static const ValentMenuEntry items[] = {
    {N_("Lock"),   "device.lock",   "phonelink-lock-symbolic"},
    {N_("Unlock"), "device.unlock", "phonelink-lock-symbolic"}
};

/*
 * ValentDevicePlugin
 */
static void
valent_lock_plugin_enable (ValentDevicePlugin *plugin)
{
  ValentLockPlugin *self = VALENT_LOCK_PLUGIN (plugin);

  g_assert (VALENT_IS_LOCK_PLUGIN (self));

  self->session = valent_session_get_default ();
  g_signal_connect_swapped (self->session,
                            "changed",
                            G_CALLBACK (valent_lock_plugin_send_state),
                            self);

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
valent_lock_plugin_disable (ValentDevicePlugin *plugin)
{
  ValentLockPlugin *self = VALENT_LOCK_PLUGIN (plugin);

  g_assert (VALENT_IS_LOCK_PLUGIN (self));

  g_signal_handlers_disconnect_by_data (self->session, self);

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
valent_lock_plugin_update_state (ValentDevicePlugin *plugin)
{
  ValentLockPlugin *self = VALENT_LOCK_PLUGIN (plugin);
  gboolean connected;
  gboolean paired;
  gboolean available;

  g_assert (VALENT_IS_LOCK_PLUGIN (self));

  connected = valent_device_get_connected (self->device);
  paired = valent_device_get_paired (self->device);
  available = (connected && paired);

  /* GActions */
  if (available)
    {
      update_actions (self);
      valent_lock_plugin_request_state (self);
    }
  else
    {
      valent_device_plugin_toggle_actions (plugin,
                                           actions, G_N_ELEMENTS (actions),
                                           available);
    }
}

static void
valent_lock_plugin_handle_packet (ValentDevicePlugin *plugin,
                                  const char         *type,
                                  JsonNode           *packet)
{
  ValentLockPlugin *self = VALENT_LOCK_PLUGIN (plugin);

  g_assert (VALENT_IS_LOCK_PLUGIN (self));
  g_assert (type != NULL);
  g_assert (VALENT_IS_PACKET (packet));

  if (g_strcmp0 (type, "kdeconnect.lock") == 0)
    valent_lock_plugin_handle_lock (self, packet);
  else if (g_strcmp0 (type, "kdeconnect.lock.request") == 0)
    valent_lock_plugin_handle_lock_request (self, packet);
  else
    g_assert_not_reached ();
}

static void
valent_device_plugin_iface_init (ValentDevicePluginInterface *iface)
{
  iface->enable = valent_lock_plugin_enable;
  iface->disable = valent_lock_plugin_disable;
  iface->handle_packet = valent_lock_plugin_handle_packet;
  iface->update_state = valent_lock_plugin_update_state;
}

/*
 * GObject
 */
static void
valent_lock_plugin_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  ValentLockPlugin *self = VALENT_LOCK_PLUGIN (object);

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
valent_lock_plugin_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  ValentLockPlugin *self = VALENT_LOCK_PLUGIN (object);

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
valent_lock_plugin_class_init (ValentLockPluginClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = valent_lock_plugin_get_property;
  object_class->set_property = valent_lock_plugin_set_property;

  g_object_class_override_property (object_class, PROP_DEVICE, "device");
}

static void
valent_lock_plugin_init (ValentLockPlugin *self)
{
}

