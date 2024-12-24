// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_INSIDE) && !defined (VALENT_COMPILATION)
# error "Only <valent.h> can be included directly."
#endif

#include <libpeas.h>

#include "valent-extension.h"

G_BEGIN_DECLS

#define VALENT_TYPE_PLUGIN (valent_plugin_get_type())

VALENT_AVAILABLE_IN_1_0
G_DECLARE_FINAL_TYPE (ValentPlugin, valent_plugin, VALENT, PLUGIN, ValentObject)

VALENT_AVAILABLE_IN_1_0
ValentPlugin   * valent_plugin_new              (ValentResource  *source,
                                                 PeasPluginInfo  *plugin_info,
                                                 GType            plugin_type,
                                                 const char      *plugin_domain);
VALENT_AVAILABLE_IN_1_0
gpointer         valent_plugin_create_extension (ValentPlugin    *plugin);
VALENT_AVAILABLE_IN_1_0
gboolean         valent_plugin_get_enabled      (ValentPlugin    *plugin);
VALENT_AVAILABLE_IN_1_0
void             valent_plugin_set_enabled      (ValentPlugin    *plugin,
                                                 gboolean         state);
VALENT_AVAILABLE_IN_1_0
gpointer         valent_plugin_get_extension    (ValentPlugin    *plugin);
VALENT_AVAILABLE_IN_1_0
void             valent_plugin_set_extension    (ValentPlugin    *plugin,
                                                 ValentExtension *extension);
VALENT_AVAILABLE_IN_1_0
PeasPluginInfo * valent_plugin_get_plugin_info  (ValentPlugin    *plugin);
VALENT_AVAILABLE_IN_1_0
ValentResource * valent_plugin_get_source       (ValentPlugin    *plugin);

G_END_DECLS

