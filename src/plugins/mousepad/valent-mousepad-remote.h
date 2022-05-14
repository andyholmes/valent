// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <adwaita.h>
#include <gtk/gtk.h>
#include <libvalent-core.h>

G_BEGIN_DECLS

#define VALENT_TYPE_MOUSEPAD_REMOTE (valent_mousepad_remote_get_type())

G_DECLARE_FINAL_TYPE (ValentMousepadRemote, valent_mousepad_remote, VALENT, MOUSEPAD_REMOTE, AdwWindow)

void   valent_mousepad_remote_echo_key     (ValentMousepadRemote *remote,
                                            const char           *key,
                                            GdkModifierType       mask);
void   valent_mousepad_remote_echo_special (ValentMousepadRemote *remote,
                                            unsigned int          keyval,
                                            GdkModifierType       mask);

G_END_DECLS
