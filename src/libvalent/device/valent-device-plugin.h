// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_INSIDE) && !defined (VALENT_COMPILATION)
# error "Only <valent.h> can be included directly."
#endif

#include <json-glib/json-glib.h>

#include "../core/valent-extension.h"
#include "valent-device.h"

G_BEGIN_DECLS

#define VALENT_TYPE_DEVICE_PLUGIN (valent_device_plugin_get_type ())

VALENT_AVAILABLE_IN_1_0
G_DECLARE_DERIVABLE_TYPE (ValentDevicePlugin, valent_device_plugin, VALENT, DEVICE_PLUGIN, ValentExtension)

struct _ValentDevicePluginClass
{
  ValentExtensionClass   parent_class;

  /* virtual functions */
  void                   (*handle_packet) (ValentDevicePlugin *plugin,
                                           const char         *type,
                                           JsonNode           *packet);
  void                   (*update_state)  (ValentDevicePlugin *plugin,
                                           ValentDeviceState   state);

  /*< private >*/
  gpointer            padding[8];
};

VALENT_AVAILABLE_IN_1_0
void   valent_device_plugin_handle_packet     (ValentDevicePlugin *plugin,
                                               const char         *type,
                                               JsonNode           *packet);
VALENT_AVAILABLE_IN_1_0
void   valent_device_plugin_queue_packet      (ValentDevicePlugin *plugin,
                                               JsonNode           *packet);
VALENT_AVAILABLE_IN_1_0
void   valent_device_plugin_update_state      (ValentDevicePlugin *plugin,
                                               ValentDeviceState   state);

/* TODO: move to extension? */
VALENT_AVAILABLE_IN_1_0
void   valent_device_plugin_show_notification (ValentDevicePlugin *plugin,
                                               const char         *id,
                                               GNotification      *notification);
VALENT_AVAILABLE_IN_1_0
void   valent_device_plugin_hide_notification (ValentDevicePlugin *plugin,
                                               const char         *id);

/* TODO: GMenuModel XML */
VALENT_AVAILABLE_IN_1_0
void   valent_device_plugin_set_menu_action   (ValentDevicePlugin *plugin,
                                               const char         *action,
                                               const char         *label,
                                               const char         *icon_name);
VALENT_AVAILABLE_IN_1_0
void   valent_device_plugin_set_menu_item     (ValentDevicePlugin *plugin,
                                               const char         *action,
                                               GMenuItem          *item);

/* Miscellaneous Helpers */
VALENT_AVAILABLE_IN_1_0
void   valent_notification_set_device_action  (GNotification      *notification,
                                               ValentDevice       *device,
                                               const char         *action,
                                               GVariant           *target);
VALENT_AVAILABLE_IN_1_0
void   valent_notification_add_device_button  (GNotification      *notification,
                                               ValentDevice       *device,
                                               const char         *label,
                                               const char         *action,
                                               GVariant           *target);

G_END_DECLS

