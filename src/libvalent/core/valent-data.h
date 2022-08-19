// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_CORE_INSIDE) && !defined (VALENT_CORE_COMPILATION)
# error "Only <libvalent-core.h> can be included directly."
#endif

#include <gio/gio.h>

#include "valent-version.h"

G_BEGIN_DECLS

#define VALENT_TYPE_DATA (valent_data_get_type())

VALENT_AVAILABLE_IN_1_0
G_DECLARE_DERIVABLE_TYPE (ValentData, valent_data, VALENT, DATA, GObject)

struct _ValentDataClass
{
  GObjectClass   parent_class;

  /*< private >*/
  gpointer       padding[8];
};

VALENT_AVAILABLE_IN_1_0
ValentData    * valent_data_new          (const char *context,
                                          ValentData *parent);
VALENT_AVAILABLE_IN_1_0
const char * valent_data_get_cache_path  (ValentData *data);
VALENT_AVAILABLE_IN_1_0
const char * valent_data_get_config_path (ValentData *data);
VALENT_AVAILABLE_IN_1_0
const char * valent_data_get_data_path   (ValentData *data);
VALENT_AVAILABLE_IN_1_0
const char * valent_data_get_context     (ValentData *data);
VALENT_AVAILABLE_IN_1_0
ValentData * valent_data_get_parent      (ValentData *data);
VALENT_AVAILABLE_IN_1_0
void         valent_data_clear_cache     (ValentData *data);
VALENT_AVAILABLE_IN_1_0
void         valent_data_clear_data      (ValentData *data);
VALENT_AVAILABLE_IN_1_0
GFile      * valent_data_new_cache_file  (ValentData *data,
                                          const char *filename);
VALENT_AVAILABLE_IN_1_0
GFile      * valent_data_new_config_file (ValentData *data,
                                          const char *filename);
VALENT_AVAILABLE_IN_1_0
GFile      * valent_data_new_data_file   (ValentData *data,
                                          const char *filename);

/* Static Utilities */
VALENT_AVAILABLE_IN_1_0
char       * valent_data_get_directory   (GUserDirectory   directory);
VALENT_AVAILABLE_IN_1_0
GFile      * valent_data_get_file        (const char      *dirname,
                                          const char      *basename,
                                          gboolean         unique);

G_END_DECLS
