// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_INSIDE) && !defined (VALENT_COMPILATION)
# error "Only <valent.h> can be included directly."
#endif

#include <gio/gio.h>

#include "valent-object.h"

G_BEGIN_DECLS

#define VALENT_TYPE_RESOURCE (valent_resource_get_type())

VALENT_AVAILABLE_IN_1_0
G_DECLARE_DERIVABLE_TYPE (ValentResource, valent_resource, VALENT, RESOURCE, ValentObject)

struct _ValentResourceClass
{
  ValentObjectClass   parent_class;

  /* virtual functions */
  void                (*update)     (ValentResource *resource,
                                     ValentResource *update);

  /*< private >*/
  gpointer            padding[8];
};

VALENT_AVAILABLE_IN_1_0
GStrv            valent_resource_get_contributor (ValentResource *resource);
VALENT_AVAILABLE_IN_1_0
void             valent_resource_set_contributor (ValentResource *resource,
                                                  GStrv           contributor);
VALENT_AVAILABLE_IN_1_0
const char     * valent_resource_get_coverage    (ValentResource *resource);
VALENT_AVAILABLE_IN_1_0
void             valent_resource_set_coverage    (ValentResource *resource,
                                                  const char     *coverage);
VALENT_AVAILABLE_IN_1_0
const char     * valent_resource_get_creator     (ValentResource *resource);
VALENT_AVAILABLE_IN_1_0
void             valent_resource_set_creator     (ValentResource *resource,
                                                  const char     *creator);
VALENT_AVAILABLE_IN_1_0
GDateTime      * valent_resource_get_date        (ValentResource *resource);
VALENT_AVAILABLE_IN_1_0
void             valent_resource_set_date        (ValentResource *resource,
                                                  GDateTime      *date);
VALENT_AVAILABLE_IN_1_0
const char     * valent_resource_get_description (ValentResource *resource);
VALENT_AVAILABLE_IN_1_0
void             valent_resource_set_description (ValentResource *resource,
                                                  const char     *description);
VALENT_AVAILABLE_IN_1_0
const char     * valent_resource_get_format      (ValentResource *resource);
VALENT_AVAILABLE_IN_1_0
void             valent_resource_set_format      (ValentResource *resource,
                                                  const char     *format);
VALENT_AVAILABLE_IN_1_0
const char     * valent_resource_get_identifier  (ValentResource *resource);
VALENT_AVAILABLE_IN_1_0
void             valent_resource_set_identifier  (ValentResource *resource,
                                                  const char     *identifier);
VALENT_AVAILABLE_IN_1_0
const char     * valent_resource_get_iri         (ValentResource *resource);
VALENT_AVAILABLE_IN_1_0
const char     * valent_resource_get_language    (ValentResource *resource);
VALENT_AVAILABLE_IN_1_0
void             valent_resource_set_language    (ValentResource *resource,
                                                  const char     *language);
VALENT_AVAILABLE_IN_1_0
const char     * valent_resource_get_publisher   (ValentResource *resource);
VALENT_AVAILABLE_IN_1_0
void             valent_resource_set_publisher   (ValentResource *resource,
                                                  const char     *publisher);
VALENT_AVAILABLE_IN_1_0
GStrv            valent_resource_get_relation    (ValentResource *resource);
VALENT_AVAILABLE_IN_1_0
void             valent_resource_set_relation    (ValentResource *resource,
                                                  GStrv           relation);
VALENT_AVAILABLE_IN_1_0
const char     * valent_resource_get_rights      (ValentResource *resource);
VALENT_AVAILABLE_IN_1_0
void             valent_resource_set_rights      (ValentResource *resource,
                                                  const char     *rights);
VALENT_AVAILABLE_IN_1_0
gpointer         valent_resource_get_root        (ValentResource *resource);
VALENT_AVAILABLE_IN_1_0
gpointer         valent_resource_get_source      (ValentResource *resource);
VALENT_AVAILABLE_IN_1_0
void             valent_resource_set_source      (ValentResource *resource,
                                                  ValentResource *source);
VALENT_AVAILABLE_IN_1_0
const char     * valent_resource_get_subject     (ValentResource *resource);
VALENT_AVAILABLE_IN_1_0
void             valent_resource_set_subject     (ValentResource *resource,
                                                  const char     *subject);
VALENT_AVAILABLE_IN_1_0
const char     * valent_resource_get_title       (ValentResource *resource);
VALENT_AVAILABLE_IN_1_0
void             valent_resource_set_title       (ValentResource *resource,
                                                  const char     *title);
VALENT_AVAILABLE_IN_1_0
const char     * valent_resource_get_type_hint   (ValentResource *resource);
VALENT_AVAILABLE_IN_1_0
void             valent_resource_set_type_hint   (ValentResource *resource,
                                                  const char     *type_hint);
VALENT_AVAILABLE_IN_1_0
void             valent_resource_update          (ValentResource *resource,
                                                  ValentResource *update);

G_END_DECLS

