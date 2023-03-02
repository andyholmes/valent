// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <valent.h>

G_BEGIN_DECLS

#define VALENT_TYPE_CONNECTIVITY_REPORT_PLUGIN (valent_connectivity_report_plugin_get_type())

G_DECLARE_FINAL_TYPE (ValentConnectivityReportPlugin, valent_connectivity_report_plugin, VALENT, CONNECTIVITY_REPORT_PLUGIN, ValentDevicePlugin)

G_END_DECLS

