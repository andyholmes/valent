// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_CORE_INSIDE) && !defined (VALENT_CORE_COMPILATION)
# error "Only <libvalent-core.h> can be included directly."
#endif

#include "libvalent-core-types.h"

#include <gio/gio.h>

G_BEGIN_DECLS

void   valent_object_notify          (gpointer      object,
                                      const char   *property_name);
void   valent_object_notify_by_pspec (gpointer      object,
                                      GParamSpec   *pspec);
void   valent_object_list_free       (gpointer      list);
void   valent_object_slist_free      (gpointer      slist);

G_END_DECLS

