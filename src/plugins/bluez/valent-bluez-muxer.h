// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <valent.h>

G_BEGIN_DECLS

#define VALENT_TYPE_BLUEZ_MUXER (valent_bluez_muxer_get_type())

G_DECLARE_FINAL_TYPE (ValentBluezMuxer, valent_bluez_muxer, VALENT, BLUEZ_MUXER, ValentObject)

ValentBluezMuxer * valent_bluez_muxer_new              (GIOStream            *base_stream);
void               valent_bluez_muxer_handshake        (ValentBluezMuxer     *muxer,
                                                        JsonNode             *identity,
                                                        GCancellable         *cancellable,
                                                        GAsyncReadyCallback   callback,
                                                        gpointer              user_data);
ValentChannel    * valent_bluez_muxer_handshake_finish (ValentBluezMuxer     *muxer,
                                                        GAsyncResult         *result,
                                                        GError              **error);
gboolean           valent_bluez_muxer_close            (ValentBluezMuxer     *muxer,
                                                        GCancellable         *cancellable,
                                                        GError              **error);
GIOStream        * valent_bluez_muxer_channel_accept   (ValentBluezMuxer     *muxer,
                                                        const char           *uuid,
                                                        GCancellable         *cancellable,
                                                        GError              **error);
gboolean           valent_bluez_muxer_channel_close    (ValentBluezMuxer     *muxer,
                                                        const char           *uuid,
                                                        GCancellable         *cancellable,
                                                        GError              **error);
gboolean           valent_bluez_muxer_channel_flush    (ValentBluezMuxer     *muxer,
                                                        const char           *uuid,
                                                        GCancellable         *cancellable,
                                                        GError              **error);
GIOStream        * valent_bluez_muxer_channel_open     (ValentBluezMuxer     *muxer,
                                                        const char           *uuid,
                                                        GCancellable         *cancellable,
                                                        GError              **error);
gssize             valent_bluez_muxer_channel_read     (ValentBluezMuxer     *muxer,
                                                        const char           *uuid,
                                                        void                 *buffer,
                                                        size_t                count,
                                                        gboolean              blocking,
                                                        GCancellable         *cancellable,
                                                        GError              **error);
gssize             valent_bluez_muxer_channel_write    (ValentBluezMuxer     *muxer,
                                                        const char           *uuid,
                                                        const void           *buffer,
                                                        size_t                count,
                                                        gboolean              blocking,
                                                        GCancellable         *cancellable,
                                                        GError              **error);
GSource          * valent_bluez_muxer_create_source    (ValentBluezMuxer     *muxer,
                                                        const char           *uuid,
                                                        GIOCondition          condition);
GIOCondition       valent_bluez_muxer_condition_check  (ValentBluezMuxer     *muxer,
                                                        const char           *uuid,
                                                        GIOCondition          condition);

G_END_DECLS
