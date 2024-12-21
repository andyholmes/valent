// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_INSIDE) && !defined (VALENT_COMPILATION)
# error "Only <valent.h> can be included directly."
#endif

#include <libtracker-sparql/tracker-sparql.h>

#include "valent-resource.h"

G_BEGIN_DECLS

#define VALENT_TYPE_DATA_SOURCE (valent_data_source_get_type())

VALENT_AVAILABLE_IN_1_0
G_DECLARE_DERIVABLE_TYPE (ValentDataSource, valent_data_source, VALENT, DATA_SOURCE, ValentResource)

struct _ValentDataSourceClass
{
  ValentResourceClass   parent_class;

  const char          * (*get_source_mode) (ValentDataSource *source);

  /*< private >*/
  gpointer              padding[8];
};

VALENT_AVAILABLE_IN_1_0
ValentDataSource        * valent_data_source_get_default                  (void);
VALENT_AVAILABLE_IN_1_0
ValentDataSource        * valent_data_source_get_local_default            (void);
VALENT_AVAILABLE_IN_1_0
void                      valent_data_source_clear_cache                  (ValentDataSource     *source);
VALENT_AVAILABLE_IN_1_0
void                      valent_data_source_clear_data                   (ValentDataSource     *source);
VALENT_AVAILABLE_IN_1_0
GFile                   * valent_data_source_get_cache_directory          (ValentDataSource     *source);
VALENT_AVAILABLE_IN_1_0
GFile                   * valent_data_source_get_cache_file               (ValentDataSource     *source,
                                                                           const char           *filename);
VALENT_AVAILABLE_IN_1_0
GFile                   * valent_data_source_get_config_directory         (ValentDataSource     *source);
VALENT_AVAILABLE_IN_1_0
GFile                   * valent_data_source_get_config_file              (ValentDataSource     *source,
                                                                           const char           *filename);
VALENT_AVAILABLE_IN_1_0
GSettingsBackend        * valent_data_source_get_settings_backend         (ValentDataSource     *source);
VALENT_AVAILABLE_IN_1_0
const char              * valent_data_source_get_source_mode              (ValentDataSource     *source);
VALENT_AVAILABLE_IN_1_0
void                      valent_data_source_get_sparql_connection        (ValentDataSource     *source,
                                                                           GCancellable         *cancellable,
                                                                           GAsyncReadyCallback   callback,
                                                                           gpointer              user_data);
VALENT_AVAILABLE_IN_1_0
TrackerSparqlConnection * valent_data_source_get_sparql_connection_finish (ValentDataSource     *source,
                                                                           GAsyncResult         *result,
                                                                           GError              **error);
VALENT_AVAILABLE_IN_1_0
TrackerSparqlConnection * valent_data_source_get_sparql_connection_sync   (ValentDataSource     *source,
                                                                           GCancellable         *cancellable,
                                                                           GError              **error);

G_END_DECLS

