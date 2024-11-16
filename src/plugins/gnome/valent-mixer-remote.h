// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define VALENT_TYPE_MIXER_REMOTE (valent_mixer_remote_get_type())

G_DECLARE_FINAL_TYPE (ValentMixerRemote, valent_mixer_remote, VALENT, MIXER_REMOTE, AdwBreakpointBin)

G_END_DECLS
