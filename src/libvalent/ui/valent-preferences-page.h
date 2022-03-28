// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_UI_INSIDE) && !defined (VALENT_UI_COMPILATION)
# error "Only <libvalent-ui.h> can be included directly."
#endif

#include <adwaita.h>
#include <libvalent-core.h>

G_BEGIN_DECLS

#define VALENT_TYPE_PREFERENCES_PAGE (valent_preferences_page_get_type ())

VALENT_AVAILABLE_IN_1_0
G_DECLARE_INTERFACE (ValentPreferencesPage, valent_preferences_page, VALENT, PREFERENCES_PAGE, AdwPreferencesPage)

struct _ValentPreferencesPageInterface
{
  GTypeInterface  g_iface;
};

G_END_DECLS

