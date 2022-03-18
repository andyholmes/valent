// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_CORE_INSIDE) && !defined (VALENT_CORE_COMPILATION)
# error "Only <libvalent-core.h> can be included directly."
#endif

#include <json-glib/json-glib.h>
#include <libpeas/peas.h>

#include "valent-device.h"

G_BEGIN_DECLS

/*
 * GMenu Helpers
 */
typedef struct
{
  char *label;
  char *action;
  char *icon_name;
} ValentMenuEntry;

#define VALENT_TYPE_DEVICE_PLUGIN (valent_device_plugin_get_type ())

VALENT_AVAILABLE_IN_1_0
G_DECLARE_DERIVABLE_TYPE (ValentDevicePlugin, valent_device_plugin, VALENT, DEVICE_PLUGIN, ValentObject)

struct _ValentDevicePluginClass
{
  ValentObjectClass   parent_class;

  /* virtual functions */
  void                (*disable)       (ValentDevicePlugin *plugin);
  void                (*enable)        (ValentDevicePlugin *plugin);
  void                (*handle_packet) (ValentDevicePlugin *plugin,
                                        const char         *type,
                                        JsonNode           *packet);
  void                (*update_state)  (ValentDevicePlugin *plugin,
                                        ValentDeviceState   state);
};

VALENT_AVAILABLE_IN_1_0
void           valent_device_plugin_disable             (ValentDevicePlugin    *plugin);
VALENT_AVAILABLE_IN_1_0
void           valent_device_plugin_enable              (ValentDevicePlugin    *plugin);
VALENT_AVAILABLE_IN_1_0
void           valent_device_plugin_handle_packet       (ValentDevicePlugin    *plugin,
                                                         const char            *type,
                                                         JsonNode              *packet);
VALENT_AVAILABLE_IN_1_0
void           valent_device_plugin_update_state        (ValentDevicePlugin    *plugin,
                                                         ValentDeviceState      state);
VALENT_AVAILABLE_IN_1_0
ValentDevice * valent_device_plugin_get_device          (ValentDevicePlugin    *plugin);
VALENT_AVAILABLE_IN_1_0
void           valent_device_plugin_queue_packet        (ValentDevicePlugin    *plugin,
                                                         JsonNode              *packet);
VALENT_AVAILABLE_IN_1_0
void           valent_device_plugin_show_notification   (ValentDevicePlugin    *plugin,
                                                         const char            *id,
                                                         GNotification         *notification);
VALENT_AVAILABLE_IN_1_0
void           valent_device_plugin_hide_notification   (ValentDevicePlugin    *plugin,
                                                         const char            *id);
VALENT_AVAILABLE_IN_1_0
void           valent_device_plugin_toggle_actions      (ValentDevicePlugin    *plugin,
                                                         gboolean               enabled);

/* Utilities */
VALENT_AVAILABLE_IN_1_0
GStrv          valent_device_plugin_get_incoming        (PeasPluginInfo        *info);
VALENT_AVAILABLE_IN_1_0
GStrv          valent_device_plugin_get_outgoing        (PeasPluginInfo        *info);
VALENT_AVAILABLE_IN_1_0
GSettings *    valent_device_plugin_new_settings        (const char            *device_id,
                                                         const char            *module_name);

/* TODO: GMenuModel XML */
VALENT_AVAILABLE_IN_1_0
int             valent_device_plugin_find_menu_item     (ValentDevicePlugin    *plugin,
                                                         const char            *attribute,
                                                         const GVariant        *value);
VALENT_AVAILABLE_IN_1_0
int             valent_device_plugin_remove_menu_item   (ValentDevicePlugin    *plugin,
                                                         const char            *attribute,
                                                         const GVariant        *value);
VALENT_AVAILABLE_IN_1_0
void            valent_device_plugin_replace_menu_item  (ValentDevicePlugin    *plugin,
                                                         GMenuItem             *item,
                                                         const char            *attribute);
VALENT_AVAILABLE_IN_1_0
void           valent_device_plugin_add_menu_entries    (ValentDevicePlugin    *plugin,
                                                         const ValentMenuEntry *entries,
                                                         int                    n_entries);
VALENT_AVAILABLE_IN_1_0
void           valent_device_plugin_remove_menu_entries (ValentDevicePlugin    *plugin,
                                                         const ValentMenuEntry *entries,
                                                         int                    n_entries);

/* Miscellaneous Helpers */
VALENT_AVAILABLE_IN_1_0
void           valent_notification_set_device_action    (GNotification         *notification,
                                                         ValentDevice          *device,
                                                         const char            *action,
                                                         GVariant              *target);

VALENT_AVAILABLE_IN_1_0
void           valent_notification_add_device_button    (GNotification         *notification,
                                                         ValentDevice          *device,
                                                         const char            *label,
                                                         const char            *action,
                                                         GVariant              *target);

G_END_DECLS

