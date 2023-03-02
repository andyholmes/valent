// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-mock-preferences-page"

#include "config.h"

#include <valent.h>

#include "valent-mock-preferences-page.h"


struct _ValentMockPreferencesPage
{
  ValentPreferencesPage  parent_instance;
};

G_DEFINE_FINAL_TYPE (ValentMockPreferencesPage, valent_mock_preferences_page, VALENT_TYPE_PREFERENCES_PAGE)


/*
 * GObject
 */
static void
valent_mock_preferences_page_class_init (ValentMockPreferencesPageClass *klass)
{
}

static void
valent_mock_preferences_page_init (ValentMockPreferencesPage *self)
{
}

