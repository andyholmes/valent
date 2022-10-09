// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-findmyphone-plugin"

#include "config.h"

#include <glib/gi18n.h>
#include <libpeas/peas.h>
#include <libvalent-core.h>
#include <libvalent-device.h>
#include <libvalent-session.h>

#include "valent-findmyphone-plugin.h"
#include "valent-findmyphone-ringer.h"


struct _ValentFindmyphonePlugin
{
  ValentDevicePlugin        parent_instance;

  ValentFindmyphoneRinger *ringer;
  ValentSession           *session;
};

G_DEFINE_TYPE (ValentFindmyphonePlugin, valent_findmyphone_plugin, VALENT_TYPE_DEVICE_PLUGIN)


static void
valent_findmyphone_plugin_handle_findmyphone_request (ValentFindmyphonePlugin *self)
{
  g_assert (VALENT_IS_FINDMYPHONE_PLUGIN (self));

  valent_session_set_locked (self->session, FALSE);
  valent_findmyphone_ringer_toggle (self->ringer, self);
}

/*
 * GActions
 */
static void
ring_action (GSimpleAction *action,
             GVariant      *parameter,
             gpointer       user_data)
{
  ValentFindmyphonePlugin *self = VALENT_FINDMYPHONE_PLUGIN (user_data);
  g_autoptr (JsonNode) packet = NULL;

  g_assert (VALENT_IS_FINDMYPHONE_PLUGIN (self));

  packet = valent_packet_new ("kdeconnect.findmyphone.request");
  valent_device_plugin_queue_packet (VALENT_DEVICE_PLUGIN (self), packet);
}

static const GActionEntry actions[] = {
    {"ring", ring_action, NULL, NULL, NULL}
};

static const ValentMenuEntry items[] = {
    {N_("Ring"), "device.findmyphone.ring", "phonelink-ring-symbolic"}
};

/*
 * ValentDevicePlugin
 */
static void
valent_findmyphone_plugin_enable (ValentDevicePlugin *plugin)
{
  ValentFindmyphonePlugin *self = VALENT_FINDMYPHONE_PLUGIN (plugin);

  g_assert (VALENT_IS_FINDMYPHONE_PLUGIN (self));

  g_action_map_add_action_entries (G_ACTION_MAP (plugin),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   plugin);
  valent_device_plugin_add_menu_entries (plugin,
                                         items,
                                         G_N_ELEMENTS (items));

  /* Acquire the ringer singleton and ensure the ValentSession component is
   * prepared. */
  self->ringer = valent_findmyphone_ringer_acquire ();
  self->session = valent_session_get_default ();
}

static void
valent_findmyphone_plugin_disable (ValentDevicePlugin *plugin)
{
  ValentFindmyphonePlugin *self = VALENT_FINDMYPHONE_PLUGIN (plugin);

  g_assert (VALENT_IS_FINDMYPHONE_PLUGIN (self));

  /* Release the ringer singleton */
  g_clear_pointer (&self->ringer, valent_findmyphone_ringer_release);
  self->session = NULL;

  valent_device_plugin_remove_menu_entries (plugin,
                                            items,
                                            G_N_ELEMENTS (items));
}

static void
valent_findmyphone_plugin_update_state (ValentDevicePlugin *plugin,
                                        ValentDeviceState   state)
{
  ValentFindmyphonePlugin *self = VALENT_FINDMYPHONE_PLUGIN (plugin);
  gboolean available;

  g_assert (VALENT_IS_FINDMYPHONE_PLUGIN (self));

  available = (state & VALENT_DEVICE_STATE_CONNECTED) != 0 &&
              (state & VALENT_DEVICE_STATE_PAIRED) != 0;

  /* Stop any ringing */
  if (!available && valent_findmyphone_ringer_is_owner (self->ringer, self))
    valent_findmyphone_ringer_hide (self->ringer);

  valent_device_plugin_toggle_actions (plugin, available);
}

static void
valent_findmyphone_plugin_handle_packet (ValentDevicePlugin *plugin,
                                         const char         *type,
                                         JsonNode           *packet)
{
  ValentFindmyphonePlugin *self = VALENT_FINDMYPHONE_PLUGIN (plugin);

  g_assert (VALENT_IS_FINDMYPHONE_PLUGIN (self));
  g_assert (type != NULL);
  g_assert (VALENT_IS_PACKET (packet));

  if (strcmp (type, "kdeconnect.findmyphone.request") == 0)
    valent_findmyphone_plugin_handle_findmyphone_request (self);
  else
    g_assert_not_reached ();
}

/*
 * GObject
 */
static void
valent_findmyphone_plugin_class_init (ValentFindmyphonePluginClass *klass)
{
  ValentDevicePluginClass *plugin_class = VALENT_DEVICE_PLUGIN_CLASS (klass);

  plugin_class->enable = valent_findmyphone_plugin_enable;
  plugin_class->disable = valent_findmyphone_plugin_disable;
  plugin_class->handle_packet = valent_findmyphone_plugin_handle_packet;
  plugin_class->update_state = valent_findmyphone_plugin_update_state;
}

static void
valent_findmyphone_plugin_init (ValentFindmyphonePlugin *self)
{
}

