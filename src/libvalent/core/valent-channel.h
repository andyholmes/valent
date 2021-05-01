// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_CORE_INSIDE) && !defined (VALENT_CORE_COMPILATION)
# error "Only <libvalent-core.h> can be included directly."
#endif

#include <gio/gio.h>
#include <json-glib/json-glib.h>

#include "valent-data.h"

G_BEGIN_DECLS

#define VALENT_TYPE_CHANNEL (valent_channel_get_type())

G_DECLARE_DERIVABLE_TYPE (ValentChannel, valent_channel, VALENT, CHANNEL, GObject)

struct _ValentChannelClass
{
  GObjectClass   parent_class;

  /* virtual functions */
  const char   * (*get_verification_key) (ValentChannel  *channel);
  GIOStream    * (*download)             (ValentChannel  *channel,
                                          JsonNode       *packet,
                                          GCancellable   *cancellable,
                                          GError        **error);
  GIOStream    * (*upload)               (ValentChannel  *channel,
                                          JsonNode       *packet,
                                          GCancellable   *cancellable,
                                          GError        **error);
  void           (*store_data)           (ValentChannel  *channel,
                                          ValentData     *data);
};

/* Properties */
GIOStream  * valent_channel_get_base_stream      (ValentChannel        *channel);
JsonNode   * valent_channel_get_identity         (ValentChannel        *channel);
JsonNode   * valent_channel_get_peer_identity    (ValentChannel        *channel);
const char * valent_channel_get_uri              (ValentChannel        *channel);
const char * valent_channel_get_verification_key (ValentChannel        *channel);

/* Packet Exchange */
void         valent_channel_read_packet          (ValentChannel        *channel,
                                                  GCancellable         *cancellable,
                                                  GAsyncReadyCallback   callback,
                                                  gpointer              user_data);
JsonNode   * valent_channel_read_packet_finish   (ValentChannel        *channel,
                                                  GAsyncResult         *result,
                                                  GError              **error);
void         valent_channel_write_packet         (ValentChannel        *channel,
                                                  JsonNode             *packet,
                                                  GCancellable         *cancellable,
                                                  GAsyncReadyCallback   callback,
                                                  gpointer              user_data);
gboolean     valent_channel_write_packet_finish  (ValentChannel        *channel,
                                                  GAsyncResult         *result,
                                                  GError              **error);
gboolean     valent_channel_close                (ValentChannel        *channel,
                                                  GCancellable         *cancellable,
                                                  GError              **error);
void         valent_channel_close_async          (ValentChannel        *channel,
                                                  GCancellable         *cancellable,
                                                  GAsyncReadyCallback   callback,
                                                  gpointer              user_data);
gboolean     valent_channel_close_finish         (ValentChannel        *channel,
                                                  GAsyncResult         *result,
                                                  GError              **error);

/* Virtual Functions */
GIOStream  * valent_channel_download             (ValentChannel        *channel,
                                                  JsonNode             *packet,
                                                  GCancellable         *cancellable,
                                                  GError              **error);
GIOStream  * valent_channel_upload               (ValentChannel        *channel,
                                                  JsonNode             *packet,
                                                  GCancellable         *cancellable,
                                                  GError              **error);
void         valent_channel_store_data           (ValentChannel        *channel,
                                                  ValentData           *data);

G_END_DECLS

