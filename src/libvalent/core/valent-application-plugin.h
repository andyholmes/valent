// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_CORE_INSIDE) && !defined (VALENT_CORE_COMPILATION)
# error "Only <libvalent-core.h> can be included directly."
#endif

#include <glib-object.h>

#include "valent-object.h"

G_BEGIN_DECLS

#define VALENT_TYPE_APPLICATION_PLUGIN (valent_application_plugin_get_type ())

VALENT_AVAILABLE_IN_1_0
G_DECLARE_DERIVABLE_TYPE (ValentApplicationPlugin, valent_application_plugin, VALENT, APPLICATION_PLUGIN, ValentObject)

struct _ValentApplicationPluginClass
{
  ValentObjectClass   parent_class;

  /* virtual functions */
  void                (*disable) (ValentApplicationPlugin *plugin);
  void                (*enable)  (ValentApplicationPlugin *plugin);
};

VALENT_AVAILABLE_IN_1_0
void   valent_application_plugin_disable (ValentApplicationPlugin *plugin);
VALENT_AVAILABLE_IN_1_0
void   valent_application_plugin_enable  (ValentApplicationPlugin *plugin);

G_END_DECLS

