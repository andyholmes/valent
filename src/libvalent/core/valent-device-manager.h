// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_CORE_INSIDE) && !defined (VALENT_CORE_COMPILATION)
# error "Only <libvalent-core.h> can be included directly."
#endif

#include <gio/gio.h>

#include "valent-data.h"
#include "valent-device.h"

G_BEGIN_DECLS

#define VALENT_TYPE_DEVICE_MANAGER (valent_device_manager_get_type())

VALENT_AVAILABLE_IN_1_0
G_DECLARE_FINAL_TYPE (ValentDeviceManager, valent_device_manager, VALENT, DEVICE_MANAGER, GObject)

VALENT_AVAILABLE_IN_1_0
void                  valent_device_manager_new              (GCancellable         *cancellable,
                                                              GAsyncReadyCallback   callback,
                                                              gpointer              user_data);
VALENT_AVAILABLE_IN_1_0
ValentDeviceManager * valent_device_manager_new_finish       (GAsyncResult         *result,
                                                              GError              **error);
VALENT_AVAILABLE_IN_1_0
ValentDeviceManager * valent_device_manager_new_sync         (GCancellable         *cancellable,
                                                              GError              **error);
VALENT_AVAILABLE_IN_1_0
ValentDevice        * valent_device_manager_get_device_by_id (ValentDeviceManager  *manager,
                                                              const char           *id);
VALENT_AVAILABLE_IN_1_0
const char          * valent_device_manager_get_id           (ValentDeviceManager  *manager);
VALENT_AVAILABLE_IN_1_0
const char          * valent_device_manager_get_name         (ValentDeviceManager  *manager);
VALENT_AVAILABLE_IN_1_0
void                  valent_device_manager_set_name         (ValentDeviceManager  *manager,
                                                              const char           *name);
VALENT_AVAILABLE_IN_1_0
void                  valent_device_manager_refresh          (ValentDeviceManager  *manager);
VALENT_AVAILABLE_IN_1_0
void                  valent_device_manager_start            (ValentDeviceManager  *manager);
VALENT_AVAILABLE_IN_1_0
void                  valent_device_manager_stop             (ValentDeviceManager  *manager);

/* D-Bus */
VALENT_AVAILABLE_IN_1_0
void                  valent_device_manager_export           (ValentDeviceManager  *manager,
                                                              GDBusConnection      *connection,
                                                              const char           *object_path);
VALENT_AVAILABLE_IN_1_0
void                  valent_device_manager_unexport         (ValentDeviceManager  *manager);

G_END_DECLS
