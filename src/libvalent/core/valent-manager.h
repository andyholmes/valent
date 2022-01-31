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

#define VALENT_TYPE_MANAGER (valent_manager_get_type())

VALENT_AVAILABLE_IN_1_0
G_DECLARE_FINAL_TYPE (ValentManager, valent_manager, VALENT, MANAGER, GObject)

VALENT_AVAILABLE_IN_1_0
void            valent_manager_new         (ValentData           *data,
                                            GCancellable         *cancellable,
                                            GAsyncReadyCallback   callback,
                                            gpointer              user_data);
VALENT_AVAILABLE_IN_1_0
ValentManager * valent_manager_new_finish  (GAsyncResult         *result,
                                            GError              **error);
VALENT_AVAILABLE_IN_1_0
ValentManager * valent_manager_new_sync    (ValentData           *data,
                                            GCancellable         *cancellable,
                                            GError              **error);
VALENT_AVAILABLE_IN_1_0
ValentDevice  * valent_manager_get_device  (ValentManager        *manager,
                                            const char           *id);
VALENT_AVAILABLE_IN_1_0
GPtrArray     * valent_manager_get_devices (ValentManager        *manager);
VALENT_AVAILABLE_IN_1_0
const char    * valent_manager_get_id      (ValentManager        *manager);
VALENT_AVAILABLE_IN_1_0
void            valent_manager_identify    (ValentManager        *manager,
                                            const char           *uri);
VALENT_AVAILABLE_IN_1_0
void            valent_manager_start       (ValentManager        *manager);
VALENT_AVAILABLE_IN_1_0
void            valent_manager_stop        (ValentManager        *manager);

/* D-Bus */
VALENT_AVAILABLE_IN_1_0
void            valent_manager_export      (ValentManager        *manager,
                                            GDBusConnection      *connection);
VALENT_AVAILABLE_IN_1_0
void            valent_manager_unexport    (ValentManager        *manager);

G_END_DECLS
