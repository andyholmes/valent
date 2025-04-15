// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include "valent-preferences-page.h"

G_BEGIN_DECLS

#define VALENT_TYPE_PREFERENCES_SYNC_PAGE (valent_preferences_sync_page_get_type())

G_DECLARE_FINAL_TYPE (ValentPreferencesSyncPage, valent_preferences_sync_page, VALENT, PREFERENCES_SYNC_PAGE, ValentPreferencesPage)

G_END_DECLS

