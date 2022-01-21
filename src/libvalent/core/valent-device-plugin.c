// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-device-plugin"

#include "config.h"

#include <gio/gio.h>
#include <json-glib/json-glib.h>
#include <libpeas/peas.h>

#include "valent-device.h"
#include "valent-device-plugin.h"
#include "valent-packet.h"
#include "valent-utils.h"


/**
 * SECTION:valentdeviceplugin
 * @short_description: Interface for device plugins
 * @title: ValentDevicePlugin
 * @stability: Unstable
 * @include: libvalent-core.h
 *
 * The #ValentDevicePlugin interface should be implemented by plugins that
 * operate in the scope of a single device. This generally means exchanging
 * packets with other devices.
 *
 * Implementations should not send packets to disconnected or unpaired devices
 * and attempting to do so will log a warning or critical, respectively. When
 * the device state changes, valent_device_plugin_update_state() will be called
 * to notify the plugin.
 *
 * # `.plugin` File
 *
 * Device plugins require extra information in the `.plugin` file to be
 * processed correctly.
 *
 * - `X-IncomingCapabilities`
 *
 *     A list of strings separated by semi-colons indicating the packets that
 *     the plugin can handle.
 *
 * - `X-OutgoingCapabilities`
 *
 *     A list of strings separated by semi-colons indicating the packets that
 *     the plugin may provide.
 *
 * # Plugin States
 *
 * Device plugins essentially have two sets of dependent conditions for being
 * enabled. Plugins are made available to the user if any of the following
 * criteria are met:
 *
 * - the device's outgoing types match any of the plugin's incoming capabilities
 * - the device's incoming types match any of the plugin's outgoing capabilities
 * - the plugin doesn't list any capabilities (eg. a non-packet based plugin)
 *
 * When a plugin becomes available, it may be enabled, disabled and configured
 * independent of the device's connected state. This allows the user to restrict
 * the device's access before pairing and configure plugins while it is offline.
 *
 * The #PeasExtension is only constructed once the plugin has been enabled, and
 * valent_device_plugin_enable() and valent_device_plugin_update_state() will be
 * called to setup the plugin. When a plugin is disabled the #PeasExtension will
 * be disposed immediately after valent_device_plugin_disable() is called.
 *
 * If the connected or paired state changes, valent_device_plugin_update_state()
 * will be called for each enabled plugin. Note that this function is not called
 * when a plugin is disabled, so plugins must ensure that they cleanup any state
 * when valent_device_plugin_disable() is called.
 */

G_DEFINE_INTERFACE (ValentDevicePlugin, valent_device_plugin, G_TYPE_OBJECT)

/**
 * ValentDevicePluginInterface:
 * @enable: the virtual function pointer for valent_device_plugin_enable()
 * @disable: the virtual function pointer for valent_device_plugin_disable()
 * @handle_packet: the virtual function pointer for valent_device_plugin_handle_packet()
 * @update_state: the virtual function pointer for valent_device_plugin_update_state()
 *
 * The virtual function table for #ValentDevicePlugin.
 */

/* LCOV_EXCL_START */
static void
valent_device_plugin_real_disable (ValentDevicePlugin *plugin)
{
}

static void
valent_device_plugin_real_enable (ValentDevicePlugin *plugin)
{
}

static void
valent_device_plugin_real_handle_packet (ValentDevicePlugin *plugin,
                                         const char         *type,
                                         JsonNode           *packet)
{
}

static void
valent_device_plugin_real_update_state (ValentDevicePlugin *plugin,
                                        ValentDeviceState   state)
{
}
/* LCOV_EXCL_STOP */

static void
valent_device_plugin_default_init (ValentDevicePluginInterface *iface)
{
  iface->disable = valent_device_plugin_real_disable;
  iface->enable = valent_device_plugin_real_enable;
  iface->handle_packet = valent_device_plugin_real_handle_packet;
  iface->update_state = valent_device_plugin_real_update_state;

  /**
   * ValentDevicePlugin:device:
   *
   * The #ValentDevice this plugin is bound to.
   */
  g_object_interface_install_property (iface,
                                       g_param_spec_object ("device",
                                                            "Device",
                                                            "Device",
                                                            VALENT_TYPE_DEVICE,
                                                            (G_PARAM_READWRITE |
                                                             G_PARAM_CONSTRUCT_ONLY |
                                                             G_PARAM_EXPLICIT_NOTIFY |
                                                             G_PARAM_STATIC_STRINGS)));
}

/**
 * valent_device_plugin_new_settings:
 * @device_id: a #ValentDevice ID
 * @module_name: a #PeasPluginInfo module name
 *
 * A convenience function for plugins to create a #GSettings object for a device
 * ID and module name.
 *
 * It is expected that @device_id is a valid string for a #GSettings path and
 * that the target #GSettingsSchema is of the form `ca.andyholmes.valent.module_name`.
 *
 * Returns: (transfer full): the new #GSettings object
 */
GSettings *
valent_device_plugin_new_settings (const char *device_id,
                                   const char *module_name)
{
  GSettingsSchemaSource *source;
  g_autoptr (GSettingsSchema) schema = NULL;
  g_autofree char *path = NULL;
  g_autofree char *schema_id = NULL;

  g_return_val_if_fail (device_id != NULL, NULL);
  g_return_val_if_fail (module_name != NULL, NULL);

  source = g_settings_schema_source_get_default ();
  schema_id = g_strdup_printf ("ca.andyholmes.valent.%s", module_name);
  schema = g_settings_schema_source_lookup (source, schema_id, TRUE);

  g_return_val_if_fail (schema != NULL, NULL);

  path = g_strdup_printf ("/ca/andyholmes/valent/device/%s/plugin/%s/",
                          device_id, module_name);

  return g_settings_new_full (schema, NULL, path);
}

/**
 * valent_device_plugin_get_incoming:
 * @info: a #ValentDevicePluginInfo
 *
 * Gets the list of incoming packets @plugin can handle.
 *
 * Returns: (transfer full) (nullable): a list of packet types
 */
GStrv
valent_device_plugin_get_incoming (PeasPluginInfo *info)
{
  const char *data;

  g_return_val_if_fail (info != NULL, NULL);

  data = peas_plugin_info_get_external_data (info, "IncomingCapabilities");

  return (data == NULL) ? NULL : g_strsplit (data, ";", -1);
}

/**
 * valent_device_plugin_get_outgoing:
 * @info: a #PeasPluginInfo
 *
 * Gets the list of outgoing packets @plugin can provide.
 *
 * Returns: (transfer full) (nullable): a list of packet types
 */
GStrv
valent_device_plugin_get_outgoing (PeasPluginInfo *info)
{
  const char *data;

  g_return_val_if_fail (info != NULL, NULL);

  data = peas_plugin_info_get_external_data (info, "OutgoingCapabilities");

  return (data == NULL) ? NULL : g_strsplit (data, ";", -1);
}

/**
 * valent_device_plugin_register_actions:
 * @plugin: a #ValentDevicePlugin
 * @entries: (array length=n_entries) (element-type GActionEntry): a pointer to
 *           the first item in an array of #GActionEntry structs
 * @n_entries: the length of @entries, or -1 if @entries is %NULL-terminated
 *
 * Register #GAction @entries with the #ValentDevice for @plugin. Each action is
 * passed @plugin as the user data for #GAction::activate.
 */
void
valent_device_plugin_register_actions (ValentDevicePlugin *plugin,
                                       const GActionEntry *entries,
                                       int                 n_entries)
{
  g_autoptr (ValentDevice) device = NULL;
  GActionMap *actions;

  g_return_if_fail (VALENT_IS_DEVICE_PLUGIN (plugin));
  g_return_if_fail (entries != NULL || n_entries == 0);

  g_object_get (plugin, "device", &device, NULL);
  actions = G_ACTION_MAP (valent_device_get_actions (device));

  g_action_map_add_action_entries (actions, entries, n_entries, plugin);
}

/**
 * valent_device_plugin_unregister_actions:
 * @plugin: a #ValentDevicePlugin
 * @entries: (array length=n_entries) (element-type GActionEntry): a pointer to
 *           the first item in an array of #GActionEntry structs
 * @n_entries: the length of @entries, or -1 if @entries is %NULL-terminated
 *
 * Unregister #GAction @entries with the #ValentDevice for @plugin.
 */
void
valent_device_plugin_unregister_actions (ValentDevicePlugin *plugin,
                                         const GActionEntry *entries,
                                         int                 n_entries)
{
  g_autoptr (ValentDevice) device = NULL;
  GActionMap *actions;

  g_return_if_fail (VALENT_IS_DEVICE_PLUGIN (plugin));
  g_return_if_fail (entries != NULL || n_entries == 0);

  g_object_get (plugin, "device", &device, NULL);
  actions = G_ACTION_MAP (valent_device_get_actions (device));

  for (int i = 0; i < n_entries; i++)
    g_action_map_remove_action (actions, entries[i].name);
}

/**
 * valent_device_plugin_toggle_actions:
 * @plugin: a #ValentDevicePlugin
 * @actions: (array length=n_entries) (element-type GActionEntry): a pointer to
 *           the first item in an array of #GActionEntry structs
 * @n_entries: the length of @entries, or -1 if @entries is %NULL-terminated
 * @state: boolean
 *
 * Enable or disable @actions for @device based on @state.
 */
void
valent_device_plugin_toggle_actions (ValentDevicePlugin *plugin,
                                     const GActionEntry *entries,
                                     int                 n_entries,
                                     gboolean            state)
{
  g_autoptr (ValentDevice) device = NULL;
  GActionMap *actions;

  g_return_if_fail (VALENT_IS_DEVICE_PLUGIN (plugin));
  g_return_if_fail (entries != NULL || n_entries == 0);

  g_object_get (plugin, "device", &device, NULL);
  actions = G_ACTION_MAP (valent_device_get_actions (device));

  for (int i = 0; i < n_entries; i++)
    {
      GAction *action;

      action = g_action_map_lookup_action (actions, entries[i].name);

      if (action != NULL)
        g_simple_action_set_enabled (G_SIMPLE_ACTION (action), state);
    }
}

/**
 * valent_device_plugin_add_menu_entries:
 * @plugin: a #ValentDevicePlugin
 * @entries: (array length=n_entries) (element-type ValentMenuEntry): a pointer
 *           to the first item in an array of #ValentMenuEntry structs
 * @n_entries: the length of @entries, or -1 if @entries is %NULL-terminated
 *
 * A convenience function for creating multiple #GMenuItem instances and adding
 * them to a #ValentDevice's #GMenu. Each action is constructed as per one
 * #ValentMenuEntry.
 */
void
valent_device_plugin_add_menu_entries (ValentDevicePlugin    *plugin,
                                       const ValentMenuEntry *entries,
                                       int                    n_entries)
{
  g_autoptr (ValentDevice) device = NULL;
  GMenu *menu;

  g_return_if_fail (VALENT_IS_DEVICE_PLUGIN (plugin));
  g_return_if_fail (entries != NULL || n_entries == 0);

  g_object_get (plugin, "device", &device, NULL);
  menu = G_MENU (valent_device_get_menu (device));

  for (int i = 0; i < n_entries; i++)
    {
      g_autoptr (GMenuItem) item = NULL;
      g_autoptr (GIcon) icon = NULL;

      icon = g_themed_icon_new (entries[i].icon_name);
      item = g_menu_item_new(entries[i].label, entries[i].action);
      g_menu_item_set_icon (item, icon);
      g_menu_item_set_attribute (item, "hidden-when", "s", "action-disabled");
      g_menu_append_item (menu, item);
    }
}

static int
_g_menu_find_action (GMenu      *menu,
                     const char *action)
{
  GMenuModel *model = G_MENU_MODEL (menu);
  int i, n_items;

  g_return_val_if_fail (G_IS_MENU (menu), -1);
  g_return_val_if_fail (action != NULL, -1);

  n_items = g_menu_model_get_n_items (model);

  for (i = 0; i < n_items; i++)
    {
      g_autofree char *item_str = NULL;

      g_menu_model_get_item_attribute (model, i, "action", "s", &item_str);

      if (g_strcmp0 (item_str, action) == 0)
        return i;
    }

  return -1;
}

/**
 * valent_device_plugin_remove_menu_entries:
 * @plugin: a #ValentDevicePlugin
 * @entries: (array length=n_entries) (element-type ValentMenuEntry): a pointer
 *           to the first item in an array of #ValentMenuEntry structs
 * @n_entries: the length of @entries, or -1 if @entries is %NULL-terminated
 *
 * A counterpart to valent_device_plugin_add_menu_entries().
 */
void
valent_device_plugin_remove_menu_entries (ValentDevicePlugin    *plugin,
                                          const ValentMenuEntry *entries,
                                          int                    n_entries)
{
  g_autoptr (ValentDevice) device = NULL;
  GMenu *menu;

  g_return_if_fail (VALENT_IS_DEVICE_PLUGIN (plugin));
  g_return_if_fail (entries != NULL || n_entries == 0);

  g_object_get (plugin, "device", &device, NULL);
  menu = G_MENU (valent_device_get_menu (device));

  for (int i = 0; i < n_entries; i++)
    {
      int index = _g_menu_find_action (menu, entries[i].action);

      if (index > -1)
        g_menu_remove (menu, index);
    }
}

/**
 * valent_device_plugin_find_menu_item:
 * @plugin: a #ValentDevicePlugin
 * @attribute: an attribute name to match
 * @value: an attribute value to match
 *
 * Search the top-level of a #GMenuModel for the index of an item with the
 * attribute @name holding @value.
 *
 * Returns: position of the item or -1 if not found
 */
int
valent_device_plugin_find_menu_item (ValentDevicePlugin *plugin,
                                     const char         *attribute,
                                     const GVariant     *value)
{
  g_autoptr (ValentDevice) device = NULL;
  GMenuModel *model;
  int i, n_items;

  g_return_val_if_fail (VALENT_IS_DEVICE_PLUGIN (plugin), -1);
  g_return_val_if_fail (attribute != NULL, -1);
  g_return_val_if_fail (value != NULL, -1);

  g_object_get (plugin, "device", &device, NULL);
  model = valent_device_get_menu (device);

  n_items = g_menu_model_get_n_items (model);

  for (i = 0; i < n_items; i++)
    {
      g_autoptr (GVariant) ivar = NULL;

      ivar = g_menu_model_get_item_attribute_value (model, i, attribute, NULL);

      if (ivar == NULL)
        continue;

      if (g_variant_equal (value, ivar))
        return i;
    }

  return -1;
}

/**
 * valent_device_plugin_remove_menu_item:
 * @plugin: a #ValentDevicePlugin
 * @attribute: an attribute name to match
 * @value: an attribute value to match
 *
 * Removes an item in @menu with a the specified attribute and value. If @menu
 * contains a top-level item with @attribute holding @value it is removed.
 *
 * Returns: the index of the removed item or -1 if not found.
 */
int
valent_device_plugin_remove_menu_item (ValentDevicePlugin *plugin,
                                       const char         *attribute,
                                       const GVariant     *value)
{
  g_autoptr (ValentDevice) device = NULL;
  GMenu *menu;
  int position;

  g_return_val_if_fail (VALENT_IS_DEVICE_PLUGIN (plugin), -1);
  g_return_val_if_fail (attribute != NULL, -1);
  g_return_val_if_fail (value != NULL, -1);

  g_object_get (plugin, "device", &device, NULL);
  menu = G_MENU (valent_device_get_menu (device));
  position = valent_device_plugin_find_menu_item (plugin, attribute, value);

  if (position > -1)
    g_menu_remove (menu, position);

  return position;
}

/**
 * valent_device_plugin_replace_menu_item:
 * @plugin: a #ValentDevicePlugin
 * @item: a #GMenuItem
 * @attribute: an attribute name to match
 *
 * Replaces an item in @menu with @item.
 *
 * If @menu contains a top-level item with the same action name as @item, it is
 * removed and @item is inserted at the same position. Otherwise @item is
 * appended to @menu.
 */
void
valent_device_plugin_replace_menu_item (ValentDevicePlugin *plugin,
                                        GMenuItem          *item,
                                        const char         *attribute)
{
  g_autoptr (ValentDevice) device = NULL;
  g_autoptr (GVariant) value = NULL;
  GMenu *menu;
  int position = -1;

  g_return_if_fail (VALENT_IS_DEVICE_PLUGIN (plugin));
  g_return_if_fail (G_IS_MENU_ITEM (item));

  g_object_get (plugin, "device", &device, NULL);
  menu = G_MENU (valent_device_get_menu (device));
  value = g_menu_item_get_attribute_value (item, attribute, NULL);

  if (value != NULL)
    position = valent_device_plugin_remove_menu_item (plugin, attribute, value);

  if (position > -1)
    g_menu_insert_item (menu, position, item);
  else
    g_menu_append_item (menu, item);
}

/**
 * valent_device_plugin_enable: (virtual enable)
 * @plugin: a #ValentDevicePlugin
 *
 * Enables the plugin.
 *
 * This function is called when the plugin is enabled by the user and should
 * prepare any persistent resources it may need. Usually this means registering
 * actions, preparing the plugin settings and other data sources.
 *
 * It is guaranteed that valent_device_plugin_disable() will be called if this
 * function has been called.
 */
void
valent_device_plugin_enable (ValentDevicePlugin *plugin)
{
  g_return_if_fail (VALENT_IS_DEVICE_PLUGIN (plugin));

  VALENT_DEVICE_PLUGIN_GET_IFACE (plugin)->enable (plugin);
}

/**
 * valent_device_plugin_disable: (virtual disable)
 * @plugin: a #ValentDevicePlugin
 *
 * Disables the plugin.
 *
 * This function is called when the plugin is disabled by the user and should
 * clean up any resources that were prepared in valent_device_plugin_enable() or
 * valent_device_plugin_update_state().
 *
 * It is guaranteed that this function will be called if
 * valent_device_plugin_enable() has been called.
 */
void
valent_device_plugin_disable (ValentDevicePlugin *plugin)
{
  g_return_if_fail (VALENT_IS_DEVICE_PLUGIN (plugin));

  VALENT_DEVICE_PLUGIN_GET_IFACE (plugin)->disable (plugin);
}

/**
 * valent_device_plugin_handle_packet: (virtual handle_packet)
 * @plugin: a #ValentDevicePlugin
 * @type: the packet type
 * @packet: a #JsonNode
 *
 * This is called when the device receives a packet type included in the
 * `X-IncomingCapabilities` field of the `.plugin` file for @plugin.
 *
 * This is optional for #ValentDevicePlugin implementations which do not
 * register any incoming capabilities, such as plugins that do not provide
 * packet-based functionality.
 */
void
valent_device_plugin_handle_packet (ValentDevicePlugin *plugin,
                                    const char         *type,
                                    JsonNode           *packet)
{
  g_return_if_fail (VALENT_IS_DEVICE_PLUGIN (plugin));
  g_return_if_fail (type != NULL);
  g_return_if_fail (VALENT_IS_PACKET (packet));

  VALENT_DEVICE_PLUGIN_GET_IFACE (plugin)->handle_packet (plugin, type, packet);
}

/**
 * valent_device_plugin_update_state: (virtual update_state)
 * @plugin: a #ValentDevicePlugin
 * @state: a #ValentDeviceState
 *
 * Updates the plugin state.
 *
 * This function is called when the device's connected or paired state changes
 * and may be used to prepare or release resources needed for packet exchange.
 * Usually this means configuring actions and event handler that may trigger
 * outgoing packets and exchanging connect-time data from the device.
 *
 * This is an optional for #ValentDevicePlugin implementations as plugins aren't
 * required to be dependent on the device state.
 */
void
valent_device_plugin_update_state (ValentDevicePlugin *plugin,
                                   ValentDeviceState   state)
{
  g_return_if_fail (VALENT_IS_DEVICE_PLUGIN (plugin));

  VALENT_DEVICE_PLUGIN_GET_IFACE (plugin)->update_state (plugin, state);
}

