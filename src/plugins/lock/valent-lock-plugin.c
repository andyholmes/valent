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
  ValentDevicePlugin  parent_instance;

  ValentSession     *session;
  unsigned long      session_changed_id;

  gboolean           remote_locked;
};

G_DEFINE_TYPE (ValentLockPlugin, valent_lock_plugin, VALENT_TYPE_DEVICE_PLUGIN)

static void valent_lock_plugin_request_state (ValentLockPlugin *self);
static void valent_lock_plugin_send_state    (ValentLockPlugin *self);


/*
 * Local Lock
 */
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

  valent_device_plugin_queue_packet (VALENT_DEVICE_PLUGIN (self), packet);
}

static void
valent_lock_plugin_handle_lock_request (ValentLockPlugin *self,
                                        JsonNode         *packet)
{
  gboolean state;

  g_assert (VALENT_IS_LOCK_PLUGIN (self));
  g_assert (VALENT_IS_PACKET (packet));

  if (valent_packet_check_field (packet, "requestLocked"))
    valent_lock_plugin_send_state (self);

  if (valent_packet_get_boolean (packet, "setLocked", &state))
    valent_session_set_locked (self->session, state);
}

/*
 * Remote Lock
 */
static void
update_actions (ValentLockPlugin *self)
{
  GAction *action;

  action = g_action_map_lookup_action (G_ACTION_MAP (self), "lock");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), !self->remote_locked);

  action = g_action_map_lookup_action (G_ACTION_MAP (self), "unlock");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), self->remote_locked);
}

static void
valent_lock_plugin_handle_lock (ValentLockPlugin *self,
                                JsonNode         *packet)
{
  g_assert (VALENT_IS_LOCK_PLUGIN (self));
  g_assert (VALENT_IS_PACKET (packet));

  if (valent_packet_get_boolean (packet, "isLocked", &self->remote_locked))
    update_actions (self);
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

  valent_device_plugin_queue_packet (VALENT_DEVICE_PLUGIN (self), packet);
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

  valent_device_plugin_queue_packet (VALENT_DEVICE_PLUGIN (self), packet);
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
    {N_("Lock"),   "device.lock.lock",   "phonelink-lock-symbolic"},
    {N_("Unlock"), "device.lock.unlock", "phonelink-lock-symbolic"}
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

  g_action_map_add_action_entries (G_ACTION_MAP (plugin),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   plugin);
  valent_device_plugin_add_menu_entries (plugin,
                                         items,
                                         G_N_ELEMENTS (items));
}

static void
valent_lock_plugin_disable (ValentDevicePlugin *plugin)
{
  ValentLockPlugin *self = VALENT_LOCK_PLUGIN (plugin);

  g_assert (VALENT_IS_LOCK_PLUGIN (self));

  /* We're about to dispose, so stop watching the session */
  g_clear_signal_handler (&self->session_changed_id, self->session);

  valent_device_plugin_remove_menu_entries (plugin,
                                            items,
                                            G_N_ELEMENTS (items));
}

static void
valent_lock_plugin_update_state (ValentDevicePlugin *plugin,
                                 ValentDeviceState   state)
{
  ValentLockPlugin *self = VALENT_LOCK_PLUGIN (plugin);
  gboolean available;

  g_assert (VALENT_IS_LOCK_PLUGIN (self));

  available = (state & VALENT_DEVICE_STATE_CONNECTED) != 0 &&
              (state & VALENT_DEVICE_STATE_PAIRED) != 0;

  if (available)
    {
      if (self->session_changed_id == 0)
        {
          self->session_changed_id =
            g_signal_connect_swapped (self->session,
                                      "changed",
                                      G_CALLBACK (valent_lock_plugin_send_state),
                                      self);
        }

      update_actions (self);
      valent_lock_plugin_request_state (self);
    }
  else
    {
      g_clear_signal_handler (&self->session_changed_id, self->session);
      valent_device_plugin_toggle_actions (plugin, available);
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

  if (strcmp (type, "kdeconnect.lock") == 0)
    valent_lock_plugin_handle_lock (self, packet);
  else if (strcmp (type, "kdeconnect.lock.request") == 0)
    valent_lock_plugin_handle_lock_request (self, packet);
  else
    g_assert_not_reached ();
}

/*
 * GObject
 */
static void
valent_lock_plugin_class_init (ValentLockPluginClass *klass)
{
  ValentDevicePluginClass *plugin_class = VALENT_DEVICE_PLUGIN_CLASS (klass);

  plugin_class->enable = valent_lock_plugin_enable;
  plugin_class->disable = valent_lock_plugin_disable;
  plugin_class->handle_packet = valent_lock_plugin_handle_packet;
  plugin_class->update_state = valent_lock_plugin_update_state;
}

static void
valent_lock_plugin_init (ValentLockPlugin *self)
{
}

