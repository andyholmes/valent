// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <libportal/portal.h>

G_BEGIN_DECLS

#define PORTAL_BUS_NAME              "org.freedesktop.portal.Desktop"
#define PORTAL_OBJECT_PATH           "/org/freedesktop/portal/desktop"
#define PORTAL_REQUEST_INTERFACE     "org.freedesktop.portal.Request"
#define PORTAL_SESSION_INTERFACE     "org.freedesktop.portal.Session"
#define PORTAL_FILECHOOSER_INTERFACE "org.freedesktop.portal.FileChooser"
#define PORTAL_PRINT_INTERFACE       "org.freedesktop.portal.Print"
#define PORTAL_SCREENSHOT_INTERFACE  "org.freedesktop.portal.Screenshot"
#define PORTAL_INHIBIT_INTERFACE     "org.freedesktop.portal.Inhibit"


XdpPortal * valent_xdp_get_default (void);

G_END_DECLS
