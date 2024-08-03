// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define VALENT_TYPE_DEVICE_PREFERENCES_DIALOG (valent_device_preferences_dialog_get_type())

G_DECLARE_FINAL_TYPE (ValentDevicePreferencesDialog, valent_device_preferences_dialog, VALENT, DEVICE_PREFERENCES_DIALOG, AdwPreferencesDialog)

G_END_DECLS
