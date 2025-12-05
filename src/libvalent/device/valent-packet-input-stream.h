// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <gio/gio.h>
#include <json-glib/json-glib.h>

G_BEGIN_DECLS

/**
 * VALENT_PACKET_DEFAULT_BUFFER_SIZE: (value 8192)
 *
 * The maximum packet size for untrusted devices.
 *
 * This also the default buffer size for [class@Valent.PacketInputStream].
 *
 * Since: 1.0
 */
#define VALENT_PACKET_DEFAULT_BUFFER_SIZE (8192)

#define VALENT_TYPE_PACKET_INPUT_STREAM (valent_packet_input_stream_get_type())

G_DECLARE_FINAL_TYPE (ValentPacketInputStream, valent_packet_input_stream, VALENT, PACKET_INPUT_STREAM, GBufferedInputStream)

ValentPacketInputStream * valent_packet_input_stream_new                (GInputStream             *base_stream);
gboolean                  valent_packet_input_stream_get_trusted        (ValentPacketInputStream  *stream);
void                      valent_packet_input_stream_set_trusted        (ValentPacketInputStream  *stream,
                                                                         gboolean                  trusted);
void                      valent_packet_input_stream_read_packet_async  (ValentPacketInputStream  *stream,
                                                                         GCancellable             *cancellable,
                                                                         GAsyncReadyCallback       callback,
                                                                         gpointer                  user_data);
JsonNode                * valent_packet_input_stream_read_packet_finish (ValentPacketInputStream  *stream,
                                                                         GAsyncResult             *result,
                                                                         GError                  **error);
JsonNode                * valent_packet_input_stream_read_packet        (ValentPacketInputStream  *stream,
                                                                         GCancellable             *cancellable,
                                                                         GError                  **error);

G_END_DECLS

