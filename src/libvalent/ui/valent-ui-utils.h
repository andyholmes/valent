// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <gdk-pixbuf/gdk-pixdata.h>

G_BEGIN_DECLS

GdkPixbuf * valent_ui_pixbuf_from_base64 (const char  *base64,
                                          GError     **error);

G_END_DECLS
