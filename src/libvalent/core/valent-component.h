// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_CORE_INSIDE) && !defined (VALENT_CORE_COMPILATION)
# error "Only <libvalent-core.h> can be included directly."
#endif

#include "libvalent-core-types.h"

#include <gio/gio.h>
#include <libpeas/peas.h>

G_BEGIN_DECLS

#define VALENT_TYPE_COMPONENT (valent_component_get_type())

G_DECLARE_DERIVABLE_TYPE (ValentComponent, valent_component, VALENT, COMPONENT, GObject)

struct _ValentComponentClass
{
  GObjectClass   parent_class;

  /* signals */
  void           (*extension_added)   (ValentComponent *component,
                                       PeasExtension   *extension);
  void           (*extension_removed) (ValentComponent *component,
                                       PeasExtension   *extension);
};

PeasExtension * valent_component_get_priority_provider (ValentComponent *component,
                                                        const char      *key);
GPtrArray     * valent_component_get_extensions        (ValentComponent *component);

G_END_DECLS

