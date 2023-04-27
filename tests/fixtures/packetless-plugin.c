// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

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

/*
 * GObject
 */
static void
valent_packetless_plugin_constructed (GObject *object)
{
  ValentDevicePlugin *plugin = VALENT_DEVICE_PLUGIN (object);

  g_action_map_add_action_entries (G_ACTION_MAP (plugin),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   plugin);
  valent_device_plugin_set_menu_action (plugin,
                                        "device.packetless.action",
                                        "Packetless Action",
                                        "dialog-information-symbolic");

  G_OBJECT_CLASS (valent_packetless_plugin_parent_class)->constructed (object);
}

static void
valent_packetless_plugin_dispose (GObject *object)
{
  ValentDevicePlugin *plugin = VALENT_DEVICE_PLUGIN (object);

  valent_device_plugin_set_menu_item (plugin, "device.packetless.action", NULL);

  G_OBJECT_CLASS (valent_packetless_plugin_parent_class)->dispose (object);
}

static void
valent_packetless_plugin_class_finalize (ValentPacketlessPluginClass *klass)
{
}

static void
valent_packetless_plugin_class_init (ValentPacketlessPluginClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = valent_packetless_plugin_constructed;
  object_class->dispose = valent_packetless_plugin_dispose;
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

