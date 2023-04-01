// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-mock-device-preferences-group"

#include "config.h"

#include <valent.h>

#include "valent-mock-device-preferences-group.h"


struct _ValentMockDevicePreferencesGroup
{
  ValentDevicePreferencesGroup  parent_instance;
};

G_DEFINE_FINAL_TYPE (ValentMockDevicePreferencesGroup, valent_mock_device_preferencs_group, VALENT_TYPE_DEVICE_PREFERENCES_GROUP)


/*
 * GObject
 */
static void
valent_mock_device_preferencs_group_class_init (ValentMockDevicePreferencesGroupClass *klass)
{
}

static void
valent_mock_device_preferencs_group_init (ValentMockDevicePreferencesGroup *self)
{
}

