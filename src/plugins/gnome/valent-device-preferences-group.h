// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <adwaita.h>
#include <valent.h>

G_BEGIN_DECLS

#define VALENT_TYPE_DEVICE_PREFERENCES_GROUP (valent_device_preferences_group_get_type ())

G_DECLARE_DERIVABLE_TYPE (ValentDevicePreferencesGroup, valent_device_preferences_group, VALENT, DEVICE_PREFERENCES_GROUP, AdwPreferencesGroup)

struct _ValentDevicePreferencesGroupClass
{
  AdwPreferencesGroupClass   parent_class;

  /*< private >*/
  gpointer                   padding[8];
};

GSettings     * valent_device_preferences_group_get_settings (ValentDevicePreferencesGroup *group);

G_END_DECLS

