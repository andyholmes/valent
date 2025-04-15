// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <adwaita.h>
#include <valent.h>

G_BEGIN_DECLS

#define VALENT_TYPE_PREFERENCES_PAGE (valent_preferences_page_get_type ())

G_DECLARE_DERIVABLE_TYPE (ValentPreferencesPage, valent_preferences_page, VALENT, PREFERENCES_PAGE, AdwPreferencesPage)

struct _ValentPreferencesPageClass
{
  AdwPreferencesPageClass   parent_class;

  /*< private >*/
  gpointer                   padding[8];
};

ValentContext * valent_preferences_page_get_context  (ValentPreferencesPage *page);
GSettings     * valent_preferences_page_get_settings (ValentPreferencesPage *page,
                                                      const char            *name);

G_END_DECLS

