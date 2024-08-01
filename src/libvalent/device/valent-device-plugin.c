// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-device-plugin"

#include "config.h"

#include <gio/gio.h>
#include <json-glib/json-glib.h>
#include <libpeas.h>
#include <libvalent-core.h>

#include "valent-device.h"
#include "valent-device-plugin.h"
#include "valent-packet.h"

#define PLUGIN_SETTINGS_KEY "X-DevicePluginSettings"


/**
 * ValentDevicePlugin:
 *
 * An abstract base class for device plugins.
 *
 * `ValentDevicePlugin` is a base class for plugins that operate in the scope of
 * a single device. This usually means communicating with other devices, however
 * plugins aren't required to be packet based.
 *
 * ## Plugin Requirements
 *
 * Device plugins essentially have two sets of dependent conditions for being
 * enabled. Plugins become available (i.e. can be enabled) when any of the
 * following are true:
 *
 * - any of the device's outgoing capabilities match any of the plugin's
 *   incoming capabilities
 * - any of the device's incoming capabilities match any of the plugin's
 *   outgoing capabilities
 * - the plugin doesn't list any capabilities (eg. a non-packet based plugin)
 *
 * When a plugin becomes available it may be enabled, disabled and configured.
 *
 * ## Plugin Actions
 *
 * `ValentDevicePlugin` implements the [iface@Gio.ActionGroup] and
 * [iface@Gio.ActionMap] interfaces, providing a simple way for plugins to
 * expose functions and states. Each [iface@Gio.Action] added to the action map
 * will be included in the device action group with the plugin's module name as
 * a prefix (eg. `share.uri`).
 *
 * If the [class@Valent.DeviceManager] is exported on D-Bus, the actions will be
 * exported along with the [class@Valent.Device].
 *
 * ## Implementation Notes
 *
 * Implementations that define `X-DevicePluginIncoming` in the `.plugin` file
 * must override [vfunc@Valent.DevicePlugin.handle_packet] to handle incoming
 * packets. Implementations that depend on the device state, especially those
 * that define `X-DevicePluginOutgoing` in the `.plugin` file, should override
 * [vfunc@Valent.DevicePlugin.update_state].
 *
 * ## `.plugin` File
 *
 * Implementations may define the following extra fields in the `.plugin` file:
 *
 * - `X-DevicePluginIncoming`
 *
 *     A list of packet types (eg. `kdeconnect.ping`) separated by semi-colons
 *     indicating the packets that the plugin can handle.
 *
 * - `X-DevicePluginOutgoing`
 *
 *     A list of packet types (eg. `kdeconnect.share.request`) separated by
 *     semi-colons indicating the packets that the plugin may send.
 *
 * - `X-DevicePluginSettings`
 *
 *     A [class@Gio.Settings] schema ID for the plugin's settings. See
 *     [method@Valent.Context.get_plugin_settings] for more information.
 *
 * Since: 1.0
 */

typedef struct
{
  gpointer  reserved[1];
} ValentDevicePluginPrivate;

G_DEFINE_ABSTRACT_TYPE (ValentDevicePlugin, valent_device_plugin, VALENT_TYPE_EXTENSION)

/**
 * ValentDevicePluginClass:
 * @handle_packet: the virtual function pointer for valent_device_plugin_handle_packet()
 * @update_state: the virtual function pointer for valent_device_plugin_update_state()
 *
 * The virtual function table for `ValentDevicePlugin`.
 */


/* LCOV_EXCL_START */
static void
valent_device_plugin_real_handle_packet (ValentDevicePlugin *plugin,
                                         const char         *type,
                                         JsonNode           *packet)
{
  g_assert (VALENT_IS_DEVICE_PLUGIN (plugin));
  g_assert (type != NULL && *type != '\0');
  g_assert (VALENT_IS_PACKET (packet));

  g_critical ("%s: expected handler for \"%s\" packet",
              G_OBJECT_TYPE_NAME (plugin),
              type);
}

static void
valent_device_plugin_real_update_state (ValentDevicePlugin *plugin,
                                        ValentDeviceState   state)
{
  g_assert (VALENT_IS_DEVICE_PLUGIN (plugin));
}
/* LCOV_EXCL_STOP */

static void
valent_device_send_packet_cb (ValentDevice *device,
                              GAsyncResult *result,
                              gpointer      user_data)
{
  g_autoptr (GError) error = NULL;

  if (!valent_device_send_packet_finish (device, result, &error))
    {
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED))
        g_critical ("%s(): %s", G_STRFUNC, error->message);
      else if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_CONNECTED))
        g_warning ("%s(): %s", G_STRFUNC, error->message);
      else if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_debug ("%s(): %s", G_STRFUNC, error->message);
    }
}

/*
 * GObject
 */
static void
valent_device_plugin_class_init (ValentDevicePluginClass *klass)
{
  klass->handle_packet = valent_device_plugin_real_handle_packet;
  klass->update_state = valent_device_plugin_real_update_state;
}

static void
valent_device_plugin_init (ValentDevicePlugin *self)
{
}

/**
 * valent_device_plugin_queue_packet:
 * @plugin: a `ValentDevicePlugin`
 * @packet: a KDE Connect packet
 *
 * Queue a KDE Connect packet to be sent to the device this plugin is bound to.
 *
 * For notification of success call [method@Valent.Extension.get_object] and
 * then [method@Valent.Device.send_packet].
 *
 * Since: 1.0
 */
void
valent_device_plugin_queue_packet (ValentDevicePlugin *plugin,
                                   JsonNode           *packet)
{
  ValentDevice *device = NULL;
  g_autoptr (GCancellable) destroy = NULL;

  g_return_if_fail (VALENT_IS_DEVICE_PLUGIN (plugin));
  g_return_if_fail (VALENT_IS_PACKET (packet));

  if ((device = valent_extension_get_object (VALENT_EXTENSION (plugin))) == NULL)
    return;

  destroy = valent_object_ref_cancellable (VALENT_OBJECT (plugin));
  valent_device_send_packet (device,
                             packet,
                             destroy,
                             (GAsyncReadyCallback)valent_device_send_packet_cb,
                             NULL);
}

/**
 * valent_device_plugin_show_notification:
 * @plugin: a `ValentDevicePlugin`
 * @id: an id for the notification
 * @notification: a `GNotification`
 *
 * A convenience for showing a local notification.
 *
 * @id will be automatically prepended with the device ID and plugin module to
 * prevent conflicting with other devices and plugins.
 *
 * Call [method@Valent.DevicePlugin.hide_notification] to make the same
 * transformation on @id and withdraw the notification.
 *
 * Since: 1.0
 */
void
valent_device_plugin_show_notification (ValentDevicePlugin *plugin,
                                        const char         *id,
                                        GNotification      *notification)
{
  GApplication *application = g_application_get_default ();
  g_autoptr (ValentDevice) device = NULL;
  g_autoptr (PeasPluginInfo) plugin_info = NULL;
  g_autofree char *notification_id = NULL;

  g_return_if_fail (VALENT_IS_DEVICE_PLUGIN (plugin));
  g_return_if_fail (id != NULL);
  g_return_if_fail (G_IS_NOTIFICATION (notification));

  if G_UNLIKELY (application == NULL)
    return;

  g_object_get (plugin,
                "object",      &device,
                "plugin-info", &plugin_info,
                NULL);
  notification_id = g_strdup_printf ("%s::%s::%s",
                                     valent_device_get_id (device),
                                     peas_plugin_info_get_module_name (plugin_info),
                                     id);
  g_application_send_notification (application, notification_id, notification);
}

/**
 * valent_device_plugin_hide_notification:
 * @plugin: a `ValentDevicePlugin`
 * @id: an id for the notification
 *
 * A convenience for withdrawing a notification.
 *
 * This method will withdraw a notification shown with
 * [method@Valent.DevicePlugin.show_notification].
 *
 * Since: 1.0
 */
void
valent_device_plugin_hide_notification (ValentDevicePlugin *plugin,
                                        const char         *id)
{
  GApplication *application = g_application_get_default ();
  g_autoptr (ValentDevice) device = NULL;
  g_autoptr (PeasPluginInfo) plugin_info = NULL;
  g_autofree char *notification_id = NULL;

  g_return_if_fail (VALENT_IS_DEVICE_PLUGIN (plugin));
  g_return_if_fail (id != NULL);

  if G_UNLIKELY (application == NULL)
    return;

  g_object_get (plugin,
                "object",      &device,
                "plugin-info", &plugin_info,
                NULL);
  notification_id = g_strdup_printf ("%s::%s::%s",
                                     valent_device_get_id (device),
                                     peas_plugin_info_get_module_name (plugin_info),
                                     id);
  g_application_withdraw_notification (application, notification_id);
}

/**
 * valent_device_plugin_handle_packet: (virtual handle_packet)
 * @plugin: a `ValentDevicePlugin`
 * @type: a KDE Connect packet type
 * @packet: a KDE Connect packet
 *
 * Handle a packet from the device the plugin is bound to.
 *
 * This is called when the device receives a packet type included in the
 * `X-DevicePluginIncoming` field of the `.plugin` file.
 *
 * This is optional for implementations which do not register any incoming
 * capabilities, such as plugins that do not provide packet-based functionality.
 *
 * Since: 1.0
 */
void
valent_device_plugin_handle_packet (ValentDevicePlugin *plugin,
                                    const char         *type,
                                    JsonNode           *packet)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_DEVICE_PLUGIN (plugin));
  g_return_if_fail (type != NULL && *type != '\0');
  g_return_if_fail (VALENT_IS_PACKET (packet));

  VALENT_DEVICE_PLUGIN_GET_CLASS (plugin)->handle_packet (plugin, type, packet);

  VALENT_EXIT;
}

/**
 * valent_device_plugin_update_state: (virtual update_state)
 * @plugin: a `ValentDevicePlugin`
 * @state: a `ValentDeviceState`
 *
 * Update the plugin based on the new state of the device.
 *
 * This function is called when the connected or paired state of the device
 * changes. This may be used to configure actions, event handlers that may
 * trigger outgoing packets and exchange connect-time data with the device.
 *
 * This is optional for all implementations as plugins aren't required to be
 * dependent on the device state.
 *
 * Since: 1.0
 */
void
valent_device_plugin_update_state (ValentDevicePlugin *plugin,
                                   ValentDeviceState   state)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_DEVICE_PLUGIN (plugin));

  VALENT_DEVICE_PLUGIN_GET_CLASS (plugin)->update_state (plugin, state);

  VALENT_EXIT;
}

static int
_g_menu_find_action (GMenuModel *menu,
                     const char *action)
{
  int i, n_items;

  g_assert (G_IS_MENU_MODEL (menu));
  g_assert (action != NULL);

  n_items = g_menu_model_get_n_items (menu);

  for (i = 0; i < n_items; i++)
    {
      g_autofree char *item_str = NULL;

      g_menu_model_get_item_attribute (menu, i, "action", "s", &item_str);

      if (g_strcmp0 (item_str, action) == 0)
        return i;
    }

  return -1;
}

/**
 * valent_device_plugin_set_menu_action:
 * @plugin: a `ValentDevicePlugin`
 * @action: a `GAction` name
 * @label: (nullable): a label for the action
 * @icon_name: (nullable): an icon for the action
 *
 * Set or remove a device menu action by [iface@Gio.Action] name.
 *
 * If @label and @icon are %NULL, @action will be removed from the menu.
 *
 * Since: 1.0
 */
void
valent_device_plugin_set_menu_action (ValentDevicePlugin *plugin,
                                      const char         *action,
                                      const char         *label,
                                      const char         *icon_name)
{
  g_autoptr (GMenuItem) item = NULL;

  g_return_if_fail (VALENT_IS_DEVICE_PLUGIN (plugin));
  g_return_if_fail (action != NULL && *action != '\0');
  g_return_if_fail ((label == NULL && icon_name == NULL) ||
                    (label != NULL && *label != '\0'));

  if (label != NULL)
    {
      g_autoptr (GIcon) icon = NULL;

      if (icon_name != NULL)
        icon = g_themed_icon_new (icon_name);

      item = g_menu_item_new (label, action);
      g_menu_item_set_icon (item, icon);
      g_menu_item_set_attribute (item, "hidden-when", "s", "action-disabled");
    }

  valent_device_plugin_set_menu_item (plugin, action, item);
}

/**
 * valent_device_plugin_set_menu_item:
 * @plugin: a `ValentDevicePlugin`
 * @action: a `GAction` name
 * @item: (nullable): a `GMenuItem`
 *
 * Set or remove a device [class@Gio.MenuItem] by [iface@Gio.Action] name.
 *
 * If @item is %NULL, @action will be removed from the menu.
 *
 * Since: 1.0
 */
void
valent_device_plugin_set_menu_item (ValentDevicePlugin *plugin,
                                    const char         *action,
                                    GMenuItem          *item)
{
  ValentDevice *device = NULL;
  GMenuModel *menu;
  int index_ = -1;

  g_return_if_fail (VALENT_IS_DEVICE_PLUGIN (plugin));
  g_return_if_fail (action != NULL && *action != '\0');
  g_return_if_fail (item == NULL || G_IS_MENU_ITEM (item));

  /* NOTE: this method may be called by plugins in their `dispose()` */
  if ((device = valent_extension_get_object (VALENT_EXTENSION (plugin))) == NULL)
    return;

  menu = valent_device_get_menu (device);
  index_ = _g_menu_find_action (menu, action);

  if (index_ > -1)
    g_menu_remove (G_MENU (menu), index_);

  if (item != NULL)
    {
      if (index_ > -1)
        g_menu_insert_item (G_MENU (menu), index_, item);
      else
        g_menu_append_item (G_MENU (menu), item);
    }
}

/**
 * valent_notification_set_device_action:
 * @notification: a `GNotification`
 * @device: a `ValentDevice`
 * @action: the device action name
 * @target: (nullable): the action target
 *
 * Set the default action for @notification. @action is wrapped in the special
 * `device` action for @device, which allows it to be activated from the `app`
 * action scope.
 *
 * Since: 1.0
 */
void
valent_notification_set_device_action (GNotification *notification,
                                       ValentDevice  *device,
                                       const char    *action,
                                       GVariant      *target)
{
  GVariantBuilder builder;

  g_return_if_fail (G_IS_NOTIFICATION (notification));
  g_return_if_fail (VALENT_IS_DEVICE (device));
  g_return_if_fail (action != NULL && *action != '\0');

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("av"));

  if (target != NULL)
    g_variant_builder_add (&builder, "v", target);

  g_notification_set_default_action_and_target (notification,
                                                "app.device",
                                                "(ssav)",
                                                valent_device_get_id (device),
                                                action,
                                                &builder);
}

/**
 * valent_notification_add_device_button:
 * @notification: a `GNotification`
 * @device: a `ValentDevice`
 * @label: the button label
 * @action: the device action name
 * @target: (nullable): the action target
 *
 * Add an action button to @notification. @action is wrapped in the special
 * `device` action for @device, which allows it to be activated from the `app`
 * action scope.
 *
 * Since: 1.0
 */
void
valent_notification_add_device_button (GNotification *notification,
                                       ValentDevice  *device,
                                       const char    *label,
                                       const char    *action,
                                       GVariant      *target)
{
  GVariantBuilder builder;

  g_return_if_fail (G_IS_NOTIFICATION (notification));
  g_return_if_fail (VALENT_IS_DEVICE (device));
  g_return_if_fail (label != NULL && *label != '\0');
  g_return_if_fail (action != NULL && *action != '\0');

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("av"));

  if (target != NULL)
    g_variant_builder_add (&builder, "v", target);

  g_notification_add_button_with_target (notification,
                                         label,
                                         "app.device",
                                         "(ssav)",
                                         valent_device_get_id (device),
                                         action,
                                         &builder);
}

