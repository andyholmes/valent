// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <valent.h>

G_BEGIN_DECLS

#define VALENT_TYPE_MUX_CONNECTION (valent_mux_connection_get_type())

G_DECLARE_FINAL_TYPE (ValentMuxConnection, valent_mux_connection, VALENT, MUX_CONNECTION, ValentObject)

ValentMuxConnection * valent_mux_connection_new              (GIOStream            *base_stream);
void                  valent_mux_connection_handshake        (ValentMuxConnection  *connection,
                                                              JsonNode             *identity,
                                                              GCancellable         *cancellable,
                                                              GAsyncReadyCallback   callback,
                                                              gpointer              user_data);
ValentChannel       * valent_mux_connection_handshake_finish (ValentMuxConnection  *connection,
                                                              GAsyncResult         *result,
                                                              GError              **error);
gboolean              valent_mux_connection_close            (ValentMuxConnection  *connection,
                                                              GCancellable         *cancellable,
                                                              GError              **error);
gssize                valent_mux_connection_read             (ValentMuxConnection  *connection,
                                                              const char           *uuid,
                                                              void                 *buffer,
                                                              size_t                count,
                                                              gboolean              blocking,
                                                              GCancellable         *cancellable,
                                                              GError              **error);
gssize                valent_mux_connection_write            (ValentMuxConnection  *connection,
                                                              const char           *uuid,
                                                              const void           *buffer,
                                                              size_t                count,
                                                              gboolean              blocking,
                                                              GCancellable         *cancellable,
                                                              GError              **error);
gboolean              valent_mux_connection_close_channel    (ValentMuxConnection  *connection,
                                                              const char           *uuid,
                                                              GCancellable         *cancellable,
                                                              GError              **error);
gboolean              valent_mux_connection_close_stream     (ValentMuxConnection  *connection,
                                                              const char           *uuid,
                                                              GIOCondition          condition,
                                                              GCancellable         *cancellable,
                                                              GError              **error);
gboolean              valent_mux_connection_flush_stream     (ValentMuxConnection  *connection,
                                                              const char           *uuid,
                                                              GCancellable         *cancellable,
                                                              GError              **error);
GIOStream           * valent_mux_connection_open_channel     (ValentMuxConnection  *connection,
                                                              const char           *uuid,
                                                              GCancellable         *cancellable,
                                                              GError              **error);
GIOStream           * valent_mux_connection_accept_channel   (ValentMuxConnection  *connection,
                                                              const char           *uuid,
                                                              GCancellable         *cancellable,
                                                              GError              **error);
GSource             * valent_mux_connection_create_source    (ValentMuxConnection  *connection,
                                                              const char           *uuid,
                                                              GIOCondition          condition);
GIOCondition          valent_mux_connection_condition_check  (ValentMuxConnection  *connection,
                                                              const char           *uuid,
                                                              GIOCondition          condition);

G_END_DECLS
