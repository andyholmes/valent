// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <gio/gio.h>

G_BEGIN_DECLS

#define VALENT_TYPE_MUX_OUTPUT_STREAM (valent_mux_output_stream_get_type())

G_DECLARE_FINAL_TYPE (ValentMuxOutputStream, valent_mux_output_stream, VALENT, MUX_OUTPUT_STREAM, GOutputStream)

G_END_DECLS

