// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_INSIDE) && !defined (VALENT_COMPILATION)
# error "Only <valent.h> can be included directly."
#endif

#include <libpeas.h>

#include "valent-object.h"

G_BEGIN_DECLS

#define VALENT_TYPE_CONTEXT (valent_context_get_type())

VALENT_AVAILABLE_IN_1_0
G_DECLARE_DERIVABLE_TYPE (ValentContext, valent_context, VALENT, CONTEXT, ValentObject)

struct _ValentContextClass
{
  ValentObjectClass   parent_class;

  /*< private >*/
  gpointer            padding[8];
};

VALENT_AVAILABLE_IN_1_0
ValentContext * valent_context_new                    (ValentContext  *parent,
                                                       const char     *domain,
                                                       const char     *id);
VALENT_AVAILABLE_IN_1_0
GFile         * valent_context_get_cache_file         (ValentContext  *context,
                                                       const char     *filename);
VALENT_AVAILABLE_IN_1_0
GFile         * valent_context_get_config_file        (ValentContext  *context,
                                                       const char     *filename);
VALENT_AVAILABLE_IN_1_0
GFile         * valent_context_get_data_file          (ValentContext  *context,
                                                       const char     *filename);
VALENT_AVAILABLE_IN_1_0
const char    * valent_context_get_domain             (ValentContext  *context);
VALENT_AVAILABLE_IN_1_0
const char    * valent_context_get_id                 (ValentContext  *context);
VALENT_AVAILABLE_IN_1_0
ValentContext * valent_context_get_parent             (ValentContext  *context);
VALENT_AVAILABLE_IN_1_0
const char    * valent_context_get_path               (ValentContext  *context);
VALENT_AVAILABLE_IN_1_0
ValentContext * valent_context_get_plugin_context     (ValentContext  *context,
                                                       PeasPluginInfo *plugin_info);
VALENT_AVAILABLE_IN_1_0
GSettings     * valent_context_get_plugin_settings    (ValentContext  *context,
                                                       PeasPluginInfo *plugin_info,
                                                       const char     *plugin_key);
VALENT_AVAILABLE_IN_1_0
ValentContext * valent_context_get_root               (ValentContext  *context);
VALENT_AVAILABLE_IN_1_0
void            valent_context_clear_cache            (ValentContext  *context);
VALENT_AVAILABLE_IN_1_0
void            valent_context_clear                  (ValentContext  *context);
VALENT_AVAILABLE_IN_1_0
GSettings     * valent_context_create_settings        (ValentContext  *context,
                                                       const char     *schema_id);

G_END_DECLS
