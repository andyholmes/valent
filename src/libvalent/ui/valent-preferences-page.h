// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_INSIDE) && !defined (VALENT_COMPILATION)
# error "Only <valent.h> can be included directly."
#endif

#include <adwaita.h>

#include "../core/valent-object.h"

G_BEGIN_DECLS

#define VALENT_TYPE_PREFERENCES_PAGE (valent_preferences_page_get_type ())

VALENT_AVAILABLE_IN_1_0
G_DECLARE_DERIVABLE_TYPE (ValentPreferencesPage, valent_preferences_page, VALENT, PREFERENCES_PAGE, AdwPreferencesPage)

struct _ValentPreferencesPageClass
{
  AdwPreferencesPageClass   parent_class;

  /*< private >*/
  gpointer                  padding[8];
};

G_END_DECLS

