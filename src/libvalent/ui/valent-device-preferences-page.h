// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_INSIDE) && !defined (VALENT_COMPILATION)
# error "Only <valent.h> can be included directly."
#endif

#include <adwaita.h>

#include "../core/valent-context.h"

G_BEGIN_DECLS

#define VALENT_TYPE_DEVICE_PREFERENCES_PAGE (valent_device_preferences_page_get_type ())

VALENT_AVAILABLE_IN_1_0
G_DECLARE_DERIVABLE_TYPE (ValentDevicePreferencesPage, valent_device_preferences_page, VALENT, DEVICE_PREFERENCES_PAGE, AdwPreferencesPage)

struct _ValentDevicePreferencesPageClass
{
  AdwPreferencesPageClass   parent_class;

  /*< private >*/
  gpointer                  padding[8];
};

VALENT_AVAILABLE_IN_1_0
ValentContext * valent_device_preferences_page_get_context  (ValentDevicePreferencesPage *page);
VALENT_AVAILABLE_IN_1_0
GSettings     * valent_device_preferences_page_get_settings (ValentDevicePreferencesPage *page);

G_END_DECLS

