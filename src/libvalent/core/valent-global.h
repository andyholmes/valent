// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_INSIDE) && !defined (VALENT_COMPILATION)
# error "Only <valent.h> can be included directly."
#endif

#include <libpeas/peas.h>

#include "valent-version.h"

G_BEGIN_DECLS

VALENT_AVAILABLE_IN_1_0
GThread    * valent_get_main_thread   (void);
VALENT_AVAILABLE_IN_1_0
PeasEngine * valent_get_plugin_engine (void);
VALENT_AVAILABLE_IN_1_0
gint64       valent_timestamp_ms      (void);

G_END_DECLS

