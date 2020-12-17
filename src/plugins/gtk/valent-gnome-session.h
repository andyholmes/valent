// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <glib-object.h>
#include <libvalent-session.h>

G_BEGIN_DECLS

#define VALENT_TYPE_GNOME_SESSION (valent_gnome_session_get_type ())

G_DECLARE_FINAL_TYPE (ValentGnomeSession, valent_gnome_session, VALENT, GNOME_SESSION, ValentSessionAdapter)

G_END_DECLS

