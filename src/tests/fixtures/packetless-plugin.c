// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <libpeas/peas.h>
#include <libvalent-core.h>

#include "packetless-plugin.h"


struct _ValentPacketlessPlugin
{
  PeasExtensionBase  parent_instance;

  ValentDevice      *device;
};

static void valent_device_plugin_iface_init (ValentDevicePluginInterface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (ValentPacketlessPlugin, valent_packetless_plugin, PEAS_TYPE_EXTENSION_BASE,
                                0,
                                G_IMPLEMENT_INTERFACE_DYNAMIC (VALENT_TYPE_DEVICE_PLUGIN, valent_device_plugin_iface_init));

enum {
  PROP_0,
  PROP_DEVICE,
  N_PROPERTIES
};


/*
 * GActions
 */
static void
packetless_action (GSimpleAction *action,
                   GVariant      *parameter,
                   gpointer       user_data)
{
}

static const GActionEntry actions[] = {
    {"packetless", packetless_action, NULL, NULL, NULL}
};

static const ValentMenuEntry items[] = {
    {"Packetless Action", "device.packetless", "dialog-information-symbolic"}
};

/*
 * ValentDevicePlugin
 */
static void
valent_packetless_plugin_enable (ValentDevicePlugin *plugin)
{
  valent_device_plugin_register_actions (plugin,
                                         actions,
                                         G_N_ELEMENTS (actions));

  valent_device_plugin_add_menu_entries (plugin,
                                         items,
                                         G_N_ELEMENTS (items));
}

static void
valent_packetless_plugin_disable (ValentDevicePlugin *plugin)
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
valent_packetless_plugin_update_state (ValentDevicePlugin *plugin,
                                       ValentDeviceState   state)
{
  ValentPacketlessPlugin *self = VALENT_PACKETLESS_PLUGIN (plugin);
  gboolean available;

  g_assert (VALENT_IS_PACKETLESS_PLUGIN (self));

  available = (state & VALENT_DEVICE_STATE_CONNECTED) != 0 &&
              (state & VALENT_DEVICE_STATE_PAIRED) != 0;

  valent_device_plugin_toggle_actions (plugin,
                                       actions,
                                       G_N_ELEMENTS (actions),
                                       available);
}

static void
valent_device_plugin_iface_init (ValentDevicePluginInterface *iface)
{
  iface->enable = valent_packetless_plugin_enable;
  iface->disable = valent_packetless_plugin_disable;
  iface->update_state = valent_packetless_plugin_update_state;
}

/*
 * GObject
 */
static void
valent_packetless_plugin_class_finalize (ValentPacketlessPluginClass *klass)
{
}

static void
valent_packetless_plugin_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  ValentPacketlessPlugin *self = VALENT_PACKETLESS_PLUGIN (object);

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
valent_packetless_plugin_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  ValentPacketlessPlugin *self = VALENT_PACKETLESS_PLUGIN (object);

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
valent_packetless_plugin_class_init (ValentPacketlessPluginClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = valent_packetless_plugin_get_property;
  object_class->set_property = valent_packetless_plugin_set_property;

  g_object_class_override_property (object_class, PROP_DEVICE, "device");
}

static void
valent_packetless_plugin_init (ValentPacketlessPlugin *self)
{
}

/*
 * Peas Implementation
 */
G_MODULE_EXPORT void
valent_packetless_plugin_register_types (PeasObjectModule *module)
{
  valent_packetless_plugin_register_type (G_TYPE_MODULE (module));

  /* Plugin Interface */
  peas_object_module_register_extension_type (module,
                                              VALENT_TYPE_DEVICE_PLUGIN,
                                              VALENT_TYPE_PACKETLESS_PLUGIN);
}

