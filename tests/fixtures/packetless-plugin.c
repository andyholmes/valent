// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <libpeas/peas.h>
#include <valent.h>

#include "packetless-plugin.h"


struct _ValentPacketlessPlugin
{
  ValentDevicePlugin  parent_instance;
};

G_DEFINE_DYNAMIC_TYPE (ValentPacketlessPlugin, valent_packetless_plugin, VALENT_TYPE_DEVICE_PLUGIN)


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
    {"action", packetless_action, NULL, NULL, NULL}
};

static const ValentMenuEntry items[] = {
    {"Packetless Action", "device.packetless.action", "dialog-information-symbolic"}
};

/*
 * ValentDevicePlugin
 */
static void
valent_packetless_plugin_enable (ValentDevicePlugin *plugin)
{
  g_assert (VALENT_IS_PACKETLESS_PLUGIN (plugin));

  g_action_map_add_action_entries (G_ACTION_MAP (plugin),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   plugin);
  valent_device_plugin_add_menu_entries (plugin,
                                         items,
                                         G_N_ELEMENTS (items));
}

static void
valent_packetless_plugin_disable (ValentDevicePlugin *plugin)
{
  g_assert (VALENT_IS_PACKETLESS_PLUGIN (plugin));

  valent_device_plugin_remove_menu_entries (plugin,
                                            items,
                                            G_N_ELEMENTS (items));
}

/*
 * GObject
 */
static void
valent_packetless_plugin_class_finalize (ValentPacketlessPluginClass *klass)
{
}

static void
valent_packetless_plugin_class_init (ValentPacketlessPluginClass *klass)
{
  ValentDevicePluginClass *plugin_class = VALENT_DEVICE_PLUGIN_CLASS (klass);

  plugin_class->enable = valent_packetless_plugin_enable;
  plugin_class->disable = valent_packetless_plugin_disable;
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

  peas_object_module_register_extension_type (module,
                                              VALENT_TYPE_DEVICE_PLUGIN,
                                              VALENT_TYPE_PACKETLESS_PLUGIN);
}

