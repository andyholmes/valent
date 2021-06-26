// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_CORE_INSIDE) && !defined (VALENT_CORE_COMPILATION)
# error "Only <libvalent-core.h> can be included directly."
#endif

#include <gio/gio.h>
#include <json-glib/json-glib.h>
#include <libpeas/peas.h>


G_BEGIN_DECLS

#define VALENT_TYPE_CHANNEL_SERVICE (valent_channel_service_get_type())

G_DECLARE_DERIVABLE_TYPE (ValentChannelService, valent_channel_service, VALENT, CHANNEL_SERVICE, GObject)

struct _ValentChannelServiceClass
{
  GObjectClass   parent_class;

  /* virtual functions */
  void           (*build_identity) (ValentChannelService *service);
  void           (*identify)       (ValentChannelService *service,
                                    const char           *target);
  void           (*start)          (ValentChannelService *service,
                                    GCancellable         *cancellable,
                                    GAsyncReadyCallback   callback,
                                    gpointer              user_data);
  void           (*stop)           (ValentChannelService *service);

  /* signals */
  void           (*channel)        (ValentChannelService *service,
                                    ValentChannel        *channel);
};

void             valent_channel_service_emit_channel    (ValentChannelService   *service,
                                                         ValentChannel          *channel);
const char     * valent_channel_service_get_id          (ValentChannelService   *service);
JsonNode       * valent_channel_service_get_identity    (ValentChannelService   *service);

void             valent_channel_service_build_identity  (ValentChannelService   *service);
void             valent_channel_service_identify        (ValentChannelService   *service,
                                                         const char             *target);
void             valent_channel_service_start           (ValentChannelService   *service,
                                                         GCancellable           *cancellable,
                                                         GAsyncReadyCallback     callback,
                                                         gpointer                user_data);
gboolean         valent_channel_service_start_finish    (ValentChannelService   *service,
                                                         GAsyncResult           *result,
                                                         GError                **error);
void             valent_channel_service_stop            (ValentChannelService   *service);
gboolean         valent_channel_service_supports_plugin (ValentChannelService   *service,
                                                         PeasPluginInfo         *info);

G_END_DECLS

