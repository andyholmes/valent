// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_INSIDE) && !defined (VALENT_COMPILATION)
# error "Only <valent.h> can be included directly."
#endif

#include <json-glib/json-glib.h>

#include "../core/valent-context.h"

G_BEGIN_DECLS

#define VALENT_TYPE_CHANNEL (valent_channel_get_type())

VALENT_AVAILABLE_IN_1_0
G_DECLARE_DERIVABLE_TYPE (ValentChannel, valent_channel, VALENT, CHANNEL, ValentObject)

struct _ValentChannelClass
{
  ValentObjectClass   parent_class;

  /* virtual functions */
  GIOStream         * (*download)             (ValentChannel        *channel,
                                               JsonNode             *packet,
                                               GCancellable         *cancellable,
                                               GError              **error);
  void                (*download_async)       (ValentChannel        *channel,
                                               JsonNode             *packet,
                                               GCancellable         *cancellable,
                                               GAsyncReadyCallback   callback,
                                               gpointer              user_data);
  GIOStream         * (*download_finish)      (ValentChannel        *channel,
                                               GAsyncResult         *result,
                                               GError              **error);
  GIOStream         * (*upload)               (ValentChannel        *channel,
                                               JsonNode             *packet,
                                               GCancellable         *cancellable,
                                               GError              **error);
  void                (*upload_async)         (ValentChannel        *channel,
                                               JsonNode             *packet,
                                               GCancellable         *cancellable,
                                               GAsyncReadyCallback   callback,
                                               gpointer              user_data);
  GIOStream         * (*upload_finish)        (ValentChannel        *channel,
                                               GAsyncResult         *result,
                                               GError              **error);

  /*< private >*/
  gpointer            padding[8];
};


VALENT_AVAILABLE_IN_1_0
GIOStream       * valent_channel_ref_base_stream      (ValentChannel        *channel);
VALENT_AVAILABLE_IN_1_0
GTlsCertificate * valent_channel_get_certificate      (ValentChannel        *channel);
VALENT_AVAILABLE_IN_1_0
JsonNode        * valent_channel_get_identity         (ValentChannel        *channel);
VALENT_AVAILABLE_IN_1_0
GTlsCertificate * valent_channel_get_peer_certificate (ValentChannel        *channel);
VALENT_AVAILABLE_IN_1_0
JsonNode        * valent_channel_get_peer_identity    (ValentChannel        *channel);
VALENT_AVAILABLE_IN_1_0
GIOStream       * valent_channel_download             (ValentChannel        *channel,
                                                       JsonNode             *packet,
                                                       GCancellable         *cancellable,
                                                       GError              **error);
VALENT_AVAILABLE_IN_1_0
void              valent_channel_download_async       (ValentChannel        *channel,
                                                       JsonNode             *packet,
                                                       GCancellable         *cancellable,
                                                       GAsyncReadyCallback   callback,
                                                       gpointer              user_data);
VALENT_AVAILABLE_IN_1_0
GIOStream       * valent_channel_download_finish      (ValentChannel        *channel,
                                                       GAsyncResult         *result,
                                                       GError              **error);
VALENT_AVAILABLE_IN_1_0
GIOStream       * valent_channel_upload               (ValentChannel        *channel,
                                                       JsonNode             *packet,
                                                       GCancellable         *cancellable,
                                                       GError              **error);
VALENT_AVAILABLE_IN_1_0
void              valent_channel_upload_async         (ValentChannel        *channel,
                                                       JsonNode             *packet,
                                                       GCancellable         *cancellable,
                                                       GAsyncReadyCallback   callback,
                                                       gpointer              user_data);
VALENT_AVAILABLE_IN_1_0
GIOStream       * valent_channel_upload_finish        (ValentChannel        *channel,
                                                       GAsyncResult         *result,
                                                       GError              **error);
VALENT_AVAILABLE_IN_1_0
void              valent_channel_read_packet          (ValentChannel        *channel,
                                                       GCancellable         *cancellable,
                                                       GAsyncReadyCallback   callback,
                                                       gpointer              user_data);
VALENT_AVAILABLE_IN_1_0
JsonNode        * valent_channel_read_packet_finish   (ValentChannel        *channel,
                                                       GAsyncResult         *result,
                                                       GError              **error);
VALENT_AVAILABLE_IN_1_0
void              valent_channel_write_packet         (ValentChannel        *channel,
                                                       JsonNode             *packet,
                                                       GCancellable         *cancellable,
                                                       GAsyncReadyCallback   callback,
                                                       gpointer              user_data);
VALENT_AVAILABLE_IN_1_0
gboolean          valent_channel_write_packet_finish  (ValentChannel        *channel,
                                                       GAsyncResult         *result,
                                                       GError              **error);
VALENT_AVAILABLE_IN_1_0
gboolean          valent_channel_close                (ValentChannel        *channel,
                                                       GCancellable         *cancellable,
                                                       GError              **error);
VALENT_AVAILABLE_IN_1_0
void              valent_channel_close_async          (ValentChannel        *channel,
                                                       GCancellable         *cancellable,
                                                       GAsyncReadyCallback   callback,
                                                       gpointer              user_data);
VALENT_AVAILABLE_IN_1_0
gboolean          valent_channel_close_finish         (ValentChannel        *channel,
                                                       GAsyncResult         *result,
                                                       GError              **error);

G_END_DECLS

