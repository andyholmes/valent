// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <valent.h>

G_BEGIN_DECLS

#define VALENT_TYPE_GNOME_APPLICATION (valent_gnome_application_get_type())

G_DECLARE_FINAL_TYPE (ValentGNOMEApplication, valent_gnome_application, VALENT, GNOME_APPLICATION, ValentApplicationPlugin)

G_END_DECLS

