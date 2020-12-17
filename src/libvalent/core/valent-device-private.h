// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <gio/gio.h>
#include <json-glib/json-glib.h>
#include <libpeas/peas.h>

#include "libvalent-core-types.h"

G_BEGIN_DECLS

void     valent_device_handle_packet   (ValentDevice   *device,
                                        JsonNode       *packet);
void     valent_device_set_channel     (ValentDevice   *device,
                                        ValentChannel  *channel);
void     valent_device_set_paired      (ValentDevice   *device,
                                        gboolean        paired);
gboolean valent_device_supports_plugin (ValentDevice   *device,
                                        PeasPluginInfo *info);

G_END_DECLS
