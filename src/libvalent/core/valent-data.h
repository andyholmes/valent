// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_CORE_INSIDE) && !defined (VALENT_CORE_COMPILATION)
# error "Only <libvalent-core.h> can be included directly."
#endif

#include <gio/gio.h>

#include "libvalent-core-types.h"

G_BEGIN_DECLS

#define VALENT_TYPE_DATA (valent_data_get_type())

G_DECLARE_DERIVABLE_TYPE (ValentData, valent_data, VALENT, DATA, GObject)

struct _ValentDataClass
{
  GObjectClass   parent_class;
};

ValentData    * valent_data_new                 (const char      *context,
                                                 ValentData      *parent);
char          * valent_data_get_directory       (GUserDirectory   directory);
GFile         * valent_data_get_file            (const char      *dirname,
                                                 const char      *basename,
                                                 gboolean         unique);

/* Properties */
GFile         * valent_data_get_cache_file      (ValentData      *data,
                                                 const char      *filename);
GFile         * valent_data_get_config_file     (ValentData      *data,
                                                 const char      *filename);
GFile         * valent_data_get_data_file       (ValentData      *data,
                                                 const char      *filename);
const char    * valent_data_get_cache_path      (ValentData      *data);
const char    * valent_data_get_config_path     (ValentData      *data);
const char    * valent_data_get_data_path       (ValentData      *data);
const char    * valent_data_get_context         (ValentData      *data);
ValentData    * valent_data_get_parent          (ValentData      *data);

/* Public Methods */
void            valent_data_clear_cache         (ValentData      *data);
void            valent_data_clear_config        (ValentData      *data);

G_END_DECLS
