// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_CORE_INSIDE) && !defined (VALENT_CORE_COMPILATION)
# error "Only <libvalent-core.h> can be included directly."
#endif

#include <gio/gio.h>
#include <libpeas/peas.h>

#include "valent-device.h"

G_BEGIN_DECLS

VALENT_AVAILABLE_IN_1_0
PeasEngine * valent_get_engine      (void);
VALENT_AVAILABLE_IN_1_0
GThread    * valent_get_main_thread (void);
VALENT_AVAILABLE_IN_1_0
gboolean     valent_in_flatpak      (void);
VALENT_AVAILABLE_IN_1_0
void         valent_load_plugins    (PeasEngine *engine);
VALENT_AVAILABLE_IN_1_0
gint64       valent_timestamp_ms    (void);


/* Miscellaneous Helpers */
VALENT_AVAILABLE_IN_1_0
void       valent_notification_set_device_action (GNotification *notification,
                                                  ValentDevice  *device,
                                                  const char    *action,
                                                  GVariant      *target);

VALENT_AVAILABLE_IN_1_0
void       valent_notification_add_device_button (GNotification *notification,
                                                  ValentDevice  *device,
                                                  const char    *label,
                                                  const char    *action,
                                                  GVariant      *target);

G_END_DECLS

