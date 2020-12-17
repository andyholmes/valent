// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-findmyphone-plugin"

#include "config.h"

#include <glib/gi18n.h>
#include <libpeas/peas.h>
#include <libvalent-core.h>
#include <libvalent-session.h>

#include "valent-findmyphone-plugin.h"
#include "valent-findmyphone-ringer.h"


struct _ValentFindmyphonePlugin
{
  PeasExtensionBase        parent_instance;

  ValentDevice            *device;
  ValentFindmyphoneRinger *ringer;
};

static void valent_device_plugin_iface_init (ValentDevicePluginInterface *iface);

G_DEFINE_TYPE_WITH_CODE (ValentFindmyphonePlugin, valent_findmyphone_plugin, PEAS_TYPE_EXTENSION_BASE,
                         G_IMPLEMENT_INTERFACE (VALENT_TYPE_DEVICE_PLUGIN, valent_device_plugin_iface_init))

enum {
  PROP_0,
  PROP_DEVICE,
  N_PROPERTIES
};


static ValentSession *session = NULL;

static void
valent_findmyphone_plugin_handle_findmyphone_request (ValentFindmyphonePlugin *self)
{
  g_assert (VALENT_IS_FINDMYPHONE_PLUGIN (self));

  valent_session_set_locked (session, FALSE);
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
  ValentFindmyphonePlugin *self = user_data;
  JsonBuilder *builder;
  g_autoptr (JsonNode) packet = NULL;

  g_assert (VALENT_IS_FINDMYPHONE_PLUGIN (self));

  builder = valent_packet_start ("kdeconnect.findmyphone.request");
  packet = valent_packet_finish (builder);

  valent_device_queue_packet (self->device, packet);
}

static const GActionEntry actions[] = {
    {"ring", ring_action, NULL, NULL, NULL}
};

static const ValentMenuEntry items[] = {
    {N_("Ring"), "device.ring", "phonelink-ring-symbolic"}
};

/*
 * ValentDevicePlugin
 */
static void
valent_findmyphone_plugin_enable (ValentDevicePlugin *plugin)
{
  ValentFindmyphonePlugin *self = VALENT_FINDMYPHONE_PLUGIN (plugin);

  g_assert (VALENT_IS_FINDMYPHONE_PLUGIN (self));

  session = valent_session_get_default ();

  /* Acquire the ringer singleton */
  self->ringer = valent_findmyphone_ringer_acquire ();

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
valent_findmyphone_plugin_disable (ValentDevicePlugin *plugin)
{
  ValentFindmyphonePlugin *self = VALENT_FINDMYPHONE_PLUGIN (plugin);

  g_assert (VALENT_IS_FINDMYPHONE_PLUGIN (self));

  /* Unregister GMenu items */
  valent_device_plugin_remove_menu_entries (plugin,
                                            items,
                                            G_N_ELEMENTS (items));

  /* Unregister GActions */
  valent_device_plugin_unregister_actions (plugin,
                                           actions,
                                           G_N_ELEMENTS (actions));

  /* Release the ringer singleton */
  g_clear_pointer (&self->ringer, valent_findmyphone_ringer_release);
}

static void
valent_findmyphone_plugin_update_state (ValentDevicePlugin *plugin)
{
  ValentFindmyphonePlugin *self = VALENT_FINDMYPHONE_PLUGIN (plugin);
  gboolean connected;
  gboolean paired;
  gboolean available;

  g_assert (VALENT_IS_FINDMYPHONE_PLUGIN (self));

  connected = valent_device_get_connected (self->device);
  paired = valent_device_get_paired (self->device);
  available = (connected && paired);

  /* GActions */
  valent_device_plugin_toggle_actions (plugin,
                                       actions, G_N_ELEMENTS (actions),
                                       available);

  /* Stop any ringing */
  if (!available && valent_findmyphone_ringer_get_owner (self->ringer) == self)
    valent_findmyphone_ringer_hide (self->ringer);
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

  if (g_strcmp0 (type, "kdeconnect.findmyphone.request") == 0)
    valent_findmyphone_plugin_handle_findmyphone_request (self);
  else
    g_assert_not_reached ();
}

static void
valent_device_plugin_iface_init (ValentDevicePluginInterface *iface)
{
  iface->enable = valent_findmyphone_plugin_enable;
  iface->disable = valent_findmyphone_plugin_disable;
  iface->handle_packet = valent_findmyphone_plugin_handle_packet;
  iface->update_state = valent_findmyphone_plugin_update_state;
}

/*
 * GObject
 */
static void
valent_findmyphone_plugin_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  ValentFindmyphonePlugin *self = VALENT_FINDMYPHONE_PLUGIN (object);

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
valent_findmyphone_plugin_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  ValentFindmyphonePlugin *self = VALENT_FINDMYPHONE_PLUGIN (object);

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
valent_findmyphone_plugin_class_init (ValentFindmyphonePluginClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = valent_findmyphone_plugin_get_property;
  object_class->set_property = valent_findmyphone_plugin_set_property;

  g_object_class_override_property (object_class, PROP_DEVICE, "device");
}

static void
valent_findmyphone_plugin_init (ValentFindmyphonePlugin *self)
{
}

