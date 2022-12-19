// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define VALENT_TYPE_MPRIS_REMOTE (valent_mpris_remote_get_type())

G_DECLARE_FINAL_TYPE (ValentMprisRemote, valent_mpris_remote, VALENT, MPRIS_REMOTE, AdwWindow)

G_END_DECLS
