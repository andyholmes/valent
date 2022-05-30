// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_CORE_INSIDE) && !defined (VALENT_CORE_COMPILATION)
# error "Only <libvalent-core.h> can be included directly."
#endif

#include <gio/gio.h>
#include <libpeas/peas.h>

#include "valent-version.h"

G_BEGIN_DECLS

#define VALENT_TYPE_COMPONENT (valent_component_get_type())

VALENT_AVAILABLE_IN_1_0
G_DECLARE_DERIVABLE_TYPE (ValentComponent, valent_component, VALENT, COMPONENT, GObject)

struct _ValentComponentClass
{
  GObjectClass   parent_class;

  /* virtual functions */
  void           (*enable_extension)  (ValentComponent *component,
                                       PeasExtension   *extension);
  void           (*disable_extension) (ValentComponent *component,
                                       PeasExtension   *extension);
};

VALENT_AVAILABLE_IN_1_0
GSettings * valent_component_create_settings (const char *context,
                                              const char *module_name);

G_END_DECLS

