// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-mock-device-preferences-page"

#include "config.h"

#include <valent.h>

#include "valent-mock-device-preferences-page.h"


struct _ValentMockDevicePreferencesPage
{
  ValentDevicePreferencesPage  parent_instance;
};

G_DEFINE_FINAL_TYPE (ValentMockDevicePreferencesPage, valent_mock_device_preferences_page, VALENT_TYPE_DEVICE_PREFERENCES_PAGE)


/*
 * GObject
 */
static void
valent_mock_device_preferences_page_class_init (ValentMockDevicePreferencesPageClass *klass)
{
}

static void
valent_mock_device_preferences_page_init (ValentMockDevicePreferencesPage *self)
{
}

