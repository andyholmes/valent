// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <gio/gio.h>

#include "valent-bluez-muxer.h"

G_BEGIN_DECLS

#define VALENT_TYPE_MUX_IO_STREAM (valent_mux_io_stream_get_type())

G_DECLARE_FINAL_TYPE (ValentMuxIOStream, valent_mux_io_stream, VALENT, MUX_IO_STREAM, GIOStream)

GIOStream * valent_mux_io_stream_new (ValentBluezMuxer *muxer,
                                      const char       *uuid);

G_END_DECLS

