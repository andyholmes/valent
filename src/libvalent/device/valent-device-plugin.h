// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_INSIDE) && !defined (VALENT_COMPILATION)
# error "Only <valent.h> can be included directly."
#endif

#include <json-glib/json-glib.h>
#include <libpeas/peas.h>

#include "valent-device.h"

G_BEGIN_DECLS

#define VALENT_TYPE_DEVICE_PLUGIN (valent_device_plugin_get_type ())

VALENT_AVAILABLE_IN_1_0
G_DECLARE_DERIVABLE_TYPE (ValentDevicePlugin, valent_device_plugin, VALENT, DEVICE_PLUGIN, ValentObject)

struct _ValentDevicePluginClass
{
  ValentObjectClass   parent_class;

  /* virtual functions */
  void                (*handle_packet) (ValentDevicePlugin *plugin,
                                        const char         *type,
                                        JsonNode           *packet);
  void                (*update_state)  (ValentDevicePlugin *plugin,
                                        ValentDeviceState   state);

  /*< private >*/
  gpointer            padding[8];
};

VALENT_AVAILABLE_IN_1_0
void            valent_device_plugin_handle_packet       (ValentDevicePlugin    *plugin,
                                                          const char            *type,
                                                          JsonNode              *packet);
VALENT_AVAILABLE_IN_1_0
void            valent_device_plugin_update_state        (ValentDevicePlugin    *plugin,
                                                          ValentDeviceState      state);
VALENT_AVAILABLE_IN_1_0
ValentContext * valent_device_plugin_get_context         (ValentDevicePlugin    *plugin);
VALENT_AVAILABLE_IN_1_0
ValentDevice  * valent_device_plugin_get_device          (ValentDevicePlugin    *plugin);
VALENT_AVAILABLE_IN_1_0
GSettings     * valent_device_plugin_get_settings        (ValentDevicePlugin    *plugin);
VALENT_AVAILABLE_IN_1_0
void            valent_device_plugin_queue_packet        (ValentDevicePlugin    *plugin,
                                                          JsonNode              *packet);
VALENT_AVAILABLE_IN_1_0
void            valent_device_plugin_show_notification   (ValentDevicePlugin    *plugin,
                                                          const char            *id,
                                                          GNotification         *notification);
VALENT_AVAILABLE_IN_1_0
void            valent_device_plugin_hide_notification   (ValentDevicePlugin    *plugin,
                                                          const char            *id);
VALENT_AVAILABLE_IN_1_0
void            valent_device_plugin_toggle_actions      (ValentDevicePlugin    *plugin,
                                                          gboolean               enabled);

/* TODO: GMenuModel XML */
VALENT_AVAILABLE_IN_1_0
void            valent_device_plugin_set_menu_action     (ValentDevicePlugin    *plugin,
                                                          const char            *action,
                                                          const char            *label,
                                                          const char            *icon_name);
VALENT_AVAILABLE_IN_1_0
void            valent_device_plugin_set_menu_item       (ValentDevicePlugin    *plugin,
                                                          const char            *action,
                                                          GMenuItem             *item);

/* Miscellaneous Helpers */
VALENT_AVAILABLE_IN_1_0
void            valent_notification_set_device_action    (GNotification         *notification,
                                                          ValentDevice          *device,
                                                          const char            *action,
                                                          GVariant              *target);
VALENT_AVAILABLE_IN_1_0
void            valent_notification_add_device_button    (GNotification         *notification,
                                                          ValentDevice          *device,
                                                          const char            *label,
                                                          const char            *action,
                                                          GVariant              *target);

G_END_DECLS

