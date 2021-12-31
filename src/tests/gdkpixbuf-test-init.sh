#!/bin/bash

# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>


# 32-bit
if command -v gdk-pixbuf-query-loaders &> /dev/null; then
    mkdir -p $(dirname ${GDK_PIXBUF_MODULE_FILE})
    gdk-pixbuf-query-loaders > $GDK_PIXBUF_MODULE_FILE

    exit 0
fi

# 64-bit
if command -v gdk-pixbuf-query-loaders-64 &> /dev/null; then
    mkdir -p $(dirname ${GDK_PIXBUF_MODULE_FILE})
    gdk-pixbuf-query-loaders-64 > $GDK_PIXBUF_MODULE_FILE

    exit 0
fi

# Report failure to meson
exit 1

