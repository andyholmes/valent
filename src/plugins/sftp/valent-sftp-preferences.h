// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <valent.h>

G_BEGIN_DECLS

#define VALENT_TYPE_SFTP_PREFERENCES (valent_sftp_preferences_get_type())

G_DECLARE_FINAL_TYPE (ValentSftpPreferences, valent_sftp_preferences, VALENT, SFTP_PREFERENCES, ValentDevicePreferencesGroup)

G_END_DECLS
