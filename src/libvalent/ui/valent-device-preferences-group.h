// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_INSIDE) && !defined (VALENT_COMPILATION)
# error "Only <valent.h> can be included directly."
#endif

#include <adwaita.h>

#include "../core/valent-context.h"

G_BEGIN_DECLS

#define VALENT_TYPE_DEVICE_PREFERENCES_GROUP (valent_device_preferences_group_get_type ())

VALENT_AVAILABLE_IN_1_0
G_DECLARE_DERIVABLE_TYPE (ValentDevicePreferencesGroup, valent_device_preferences_group, VALENT, DEVICE_PREFERENCES_GROUP, AdwPreferencesGroup)

struct _ValentDevicePreferencesGroupClass
{
  AdwPreferencesGroupClass   parent_class;

  /*< private >*/
  gpointer                   padding[8];
};

VALENT_AVAILABLE_IN_1_0
ValentContext * valent_device_preferences_group_get_context   (ValentDevicePreferencesGroup *group);
VALENT_AVAILABLE_IN_1_0
GSettings     * valent_device_preferences_group_get_settings (ValentDevicePreferencesGroup *group);

G_END_DECLS

