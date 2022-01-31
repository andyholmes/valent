// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <gdk-pixbuf/gdk-pixdata.h>
#include <libvalent-core.h>

G_BEGIN_DECLS

VALENT_AVAILABLE_IN_1_0
GdkPixbuf * valent_ui_pixbuf_from_base64 (const char  *base64,
                                          GError     **error);
VALENT_AVAILABLE_IN_1_0
char      * valent_ui_timestamp          (gint64       timestamp);
VALENT_AVAILABLE_IN_1_0
char      * valent_ui_timestamp_short    (gint64       timestamp);

G_END_DECLS
