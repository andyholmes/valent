// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_CORE_INSIDE) && !defined (VALENT_CORE_COMPILATION)
# error "Only <libvalent-core.h> can be included directly."
#endif

#include "libvalent-core-types.h"

#include <gio/gio.h>

G_BEGIN_DECLS

#define VALENT_TYPE_MANAGER (valent_manager_get_type())

G_DECLARE_FINAL_TYPE (ValentManager, valent_manager, VALENT, MANAGER, GObject)

void            valent_manager_new         (ValentData           *data,
                                            GCancellable         *cancellable,
                                            GAsyncReadyCallback   callback,
                                            gpointer              user_data);
ValentManager * valent_manager_new_finish  (GAsyncResult         *result,
                                            GError              **error);
ValentManager * valent_manager_new_sync    (ValentData           *data,
                                            GCancellable         *cancellable,
                                            GError              **error);

ValentDevice  * valent_manager_get_device  (ValentManager        *manager,
                                            const char           *id);
GPtrArray     * valent_manager_get_devices (ValentManager        *manager);
const char    * valent_manager_get_id      (ValentManager        *manager);
void            valent_manager_identify    (ValentManager        *manager,
                                            const char           *uri);
void            valent_manager_start       (ValentManager        *manager);
void            valent_manager_stop        (ValentManager        *manager);

/* D-Bus */
void            valent_manager_export      (ValentManager        *manager,
                                            GDBusConnection      *connection);
void            valent_manager_unexport    (ValentManager        *manager);

G_END_DECLS
