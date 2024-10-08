// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define VALENT_TYPE_MEDIA_REMOTE (valent_media_remote_get_type())

G_DECLARE_FINAL_TYPE (ValentMediaRemote, valent_media_remote, VALENT, MEDIA_REMOTE, AdwBreakpointBin)

G_END_DECLS
