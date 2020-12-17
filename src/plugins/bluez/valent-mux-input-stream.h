// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <gio/gio.h>

G_BEGIN_DECLS

#define VALENT_TYPE_MUX_INPUT_STREAM (valent_mux_input_stream_get_type())

G_DECLARE_FINAL_TYPE (ValentMuxInputStream, valent_mux_input_stream, VALENT, MUX_INPUT_STREAM, GInputStream)

G_END_DECLS

