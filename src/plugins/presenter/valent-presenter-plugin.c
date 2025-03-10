// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-presenter-plugin"

#include "config.h"

#include <glib/gi18n.h>
#include <gio/gio.h>
#include <json-glib/json-glib.h>
#include <valent.h>

#include "valent-presenter-plugin.h"


struct _ValentPresenterPlugin
{
  ValentDevicePlugin  parent_instance;

  ValentInput        *input;
};

G_DEFINE_FINAL_TYPE (ValentPresenterPlugin, valent_presenter_plugin, VALENT_TYPE_DEVICE_PLUGIN)


static void
valent_presenter_plugin_handle_presenter (ValentPresenterPlugin *self,
                                          JsonNode              *packet)
{
  double dx, dy;
  gboolean stop;

  g_assert (VALENT_IS_PRESENTER_PLUGIN (self));
  g_assert (VALENT_IS_PACKET (packet));

  /* NOTE: these are gyroscope motion deltas, but they're translated to pointer
   *       deltas due to lack of a virtual "laser pointer". */
  if (valent_packet_get_double (packet, "dx", &dx) &&
      valent_packet_get_double (packet, "dy", &dy))
    {
      valent_input_pointer_motion (self->input, dx * 1000, dy * 1000);
      return;
    }

  /* NOTE: this signifies that no more gyroscope deltas are incoming, so the
   *       "laser pointer" can be turned off. */
  if (valent_packet_get_boolean (packet, "stop", &stop))
    {
      VALENT_NOTE ("The \"stop\" field is not supported");
      return;
    }
}

static void
valent_presenter_plugin_send_motion (ValentPresenterPlugin *self,
                                     double                 dx,
                                     double                 dy)
{
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonNode) packet = NULL;

  g_assert (VALENT_IS_PRESENTER_PLUGIN (self));

  valent_packet_init (&builder, "kdeconnect.presenter");
  json_builder_set_member_name (builder, "dx");
  json_builder_add_double_value (builder, dx);
  json_builder_set_member_name (builder, "dy");
  json_builder_add_double_value (builder, dy);
  packet = valent_packet_end (&builder);

  valent_device_plugin_queue_packet (VALENT_DEVICE_PLUGIN (self), packet);
}

static void
valent_presenter_plugin_send_stop (ValentPresenterPlugin *self)
{
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonNode) packet = NULL;

  g_assert (VALENT_IS_PRESENTER_PLUGIN (self));

  valent_packet_init (&builder, "kdeconnect.presenter");
  json_builder_set_member_name (builder, "stop");
  json_builder_add_boolean_value (builder, TRUE);
  packet = valent_packet_end (&builder);

  valent_device_plugin_queue_packet (VALENT_DEVICE_PLUGIN (self), packet);
}

/*
 * GActions
 */
static void
valent_presenter_plugin_toggle_actions (ValentPresenterPlugin *self,
                                        gboolean               available)
{
  GAction *action;

  g_assert (VALENT_IS_PRESENTER_PLUGIN (self));

  action = g_action_map_lookup_action (G_ACTION_MAP (self), "pointer");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), available);
}

static void
presenter_pointer_action (GSimpleAction *action,
                          GVariant      *parameter,
                          gpointer       user_data)
{
  ValentPresenterPlugin *self = VALENT_PRESENTER_PLUGIN (user_data);
  double dx = 0.0;
  double dy = 0.0;
  unsigned int mask = 0;

  g_assert (VALENT_IS_PRESENTER_PLUGIN (self));

  g_variant_get (parameter, "(ddu)", &dx, &dy, &mask);

  if (!G_APPROX_VALUE (dx, 0.0, 0.01) || !G_APPROX_VALUE (dy, 0.0, 0.01))
    valent_presenter_plugin_send_motion (self, dx, dy);

  if (mask != 0)
    valent_presenter_plugin_send_stop (self);
}

static const GActionEntry actions[] = {
    {"pointer", presenter_pointer_action, "(ddu)", NULL, NULL}
};

/*
 * ValentDevicePlugin
 */
static void
valent_presenter_plugin_update_state (ValentDevicePlugin *plugin,
                                      ValentDeviceState   state)
{
  ValentPresenterPlugin *self = VALENT_PRESENTER_PLUGIN (plugin);

  gboolean available;

  g_assert (VALENT_IS_PRESENTER_PLUGIN (self));

  available = (state & VALENT_DEVICE_STATE_CONNECTED) != 0 &&
              (state & VALENT_DEVICE_STATE_PAIRED) != 0;

  valent_presenter_plugin_toggle_actions (self, available);
}

static void
valent_presenter_plugin_handle_packet (ValentDevicePlugin *plugin,
                                       const char         *type,
                                       JsonNode           *packet)
{
  ValentPresenterPlugin *self = VALENT_PRESENTER_PLUGIN (plugin);

  g_assert (VALENT_IS_PRESENTER_PLUGIN (self));
  g_assert (type != NULL);
  g_assert (VALENT_IS_PACKET (packet));

  if (g_str_equal (type, "kdeconnect.presenter"))
    valent_presenter_plugin_handle_presenter (self, packet);
  else
    g_assert_not_reached ();
}

/*
 * GObject
 */
static void
valent_presenter_plugin_constructed (GObject *object)
{
  ValentPresenterPlugin *self = VALENT_PRESENTER_PLUGIN (object);
  ValentDevicePlugin *plugin = VALENT_DEVICE_PLUGIN (object);

  G_OBJECT_CLASS (valent_presenter_plugin_parent_class)->constructed (object);

  self->input = valent_input_get_default ();
  g_action_map_add_action_entries (G_ACTION_MAP (plugin),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   plugin);
}

static void
valent_presenter_plugin_class_init (ValentPresenterPluginClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentDevicePluginClass *plugin_class = VALENT_DEVICE_PLUGIN_CLASS (klass);

  object_class->constructed = valent_presenter_plugin_constructed;

  plugin_class->handle_packet = valent_presenter_plugin_handle_packet;
  plugin_class->update_state = valent_presenter_plugin_update_state;
}

static void
valent_presenter_plugin_init (ValentPresenterPlugin *self)
{
}

