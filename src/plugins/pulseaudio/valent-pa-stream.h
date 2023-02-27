// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <valent.h>

G_BEGIN_DECLS

#define VALENT_TYPE_PA_STREAM (valent_pa_stream_get_type ())

G_DECLARE_FINAL_TYPE (ValentPaStream, valent_pa_stream, VALENT, PA_STREAM, ValentMixerStream)

G_END_DECLS

