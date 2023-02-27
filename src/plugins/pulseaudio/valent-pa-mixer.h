// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <valent.h>

G_BEGIN_DECLS

#define VALENT_TYPE_PA_MIXER (valent_pa_mixer_get_type ())

G_DECLARE_FINAL_TYPE (ValentPaMixer, valent_pa_mixer, VALENT, PA_MIXER, ValentMixerAdapter)

G_END_DECLS

