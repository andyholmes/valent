// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <gio/gio.h>

G_BEGIN_DECLS

/**
 * VALENT_BLUEZ_PROFILE_PATH: (value "/ca/andyholmes/Valent/Bluez")
 *
 * The object path for the KDE Connect bluetooth profile.
 */
#define VALENT_BLUEZ_PROFILE_PATH "/ca/andyholmes/Valent/Bluez"

/**
 * VALENT_BLUEZ_PROFILE_UUID: (value "185f3df4-3268-4e3f-9fca-d4d5059915bd")
 *
 * The service UUID for the KDE Connect bluetooth profile.
 */
#define VALENT_BLUEZ_PROFILE_UUID "185f3df4-3268-4e3f-9fca-d4d5059915bd"


#define VALENT_TYPE_BLUEZ_PROFILE (valent_bluez_profile_get_type())

G_DECLARE_FINAL_TYPE (ValentBluezProfile, valent_bluez_profile, VALENT, BLUEZ_PROFILE, GDBusInterfaceSkeleton)

gboolean valent_bluez_profile_register   (ValentBluezProfile  *profile,
                                          GDBusConnection     *connection,
                                          GCancellable        *cancellable,
                                          GError             **error);
void     valent_bluez_profile_unregister (ValentBluezProfile  *profile);

G_END_DECLS

