// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-findmyphone-plugin"

#include "config.h"

#include <glib/gi18n.h>
#include <gio/gio.h>
#include <json-glib/json-glib.h>
#include <valent.h>

#include "valent-findmyphone-plugin.h"
#include "valent-findmyphone-ringer.h"


struct _ValentFindmyphonePlugin
{
  ValentDevicePlugin       parent_instance;

  ValentFindmyphoneRinger *ringer;
  ValentSession           *session;
};

G_DEFINE_FINAL_TYPE (ValentFindmyphonePlugin, valent_findmyphone_plugin, VALENT_TYPE_DEVICE_PLUGIN)


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

/*
 * ValentDevicePlugin
 */
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

  if (g_str_equal (type, "kdeconnect.findmyphone.request"))
    valent_findmyphone_plugin_handle_findmyphone_request (self);
  else
    g_assert_not_reached ();
}

/*
 * GObject
 */
static void
valent_findmyphone_plugin_constructed (GObject *object)
{
  ValentDevicePlugin *plugin = VALENT_DEVICE_PLUGIN (object);
  ValentFindmyphonePlugin *self = VALENT_FINDMYPHONE_PLUGIN (object);

  g_action_map_add_action_entries (G_ACTION_MAP (plugin),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   plugin);
  valent_device_plugin_set_menu_action (plugin,
                                        "device.findmyphone.ring",
                                        _("Ring"),
                                        "phonelink-ring-symbolic");

  /* Acquire the ringer singleton and ensure the ValentSession component is
   * prepared. */
  self->ringer = valent_findmyphone_ringer_acquire ();
  self->session = valent_session_get_default ();

  G_OBJECT_CLASS (valent_findmyphone_plugin_parent_class)->constructed (object);
}

static void
valent_findmyphone_plugin_dispose (GObject *object)
{
  ValentFindmyphonePlugin *self = VALENT_FINDMYPHONE_PLUGIN (object);
  ValentDevicePlugin *plugin = VALENT_DEVICE_PLUGIN (object);

  /* Release the ringer singleton */
  g_clear_pointer (&self->ringer, valent_findmyphone_ringer_release);
  self->session = NULL;

  valent_device_plugin_set_menu_item (plugin, "device.findmyphone.ring", NULL);

  G_OBJECT_CLASS (valent_findmyphone_plugin_parent_class)->dispose (object);
}

static void
valent_findmyphone_plugin_class_init (ValentFindmyphonePluginClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentDevicePluginClass *plugin_class = VALENT_DEVICE_PLUGIN_CLASS (klass);

  object_class->constructed = valent_findmyphone_plugin_constructed;
  object_class->dispose = valent_findmyphone_plugin_dispose;

  plugin_class->handle_packet = valent_findmyphone_plugin_handle_packet;
  plugin_class->update_state = valent_findmyphone_plugin_update_state;
}

static void
valent_findmyphone_plugin_init (ValentFindmyphonePlugin *self)
{
}

