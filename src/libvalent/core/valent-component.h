// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_INSIDE) && !defined (VALENT_COMPILATION)
# error "Only <valent.h> can be included directly."
#endif

#include <gio/gio.h>
#include <libpeas.h>

#include "valent-object.h"

G_BEGIN_DECLS

#define VALENT_TYPE_COMPONENT (valent_component_get_type())

VALENT_AVAILABLE_IN_1_0
G_DECLARE_DERIVABLE_TYPE (ValentComponent, valent_component, VALENT, COMPONENT, ValentObject)

struct _ValentComponentClass
{
  ValentObjectClass   parent_class;

  /* virtual functions */
  void                (*bind_extension)   (ValentComponent *component,
                                           GObject         *extension);
  void                (*unbind_extension) (ValentComponent *component,
                                           GObject         *extension);
  void                (*bind_preferred)   (ValentComponent *component,
                                           GObject         *extension);

  /*< private >*/
  gpointer            padding[8];
};

G_END_DECLS

