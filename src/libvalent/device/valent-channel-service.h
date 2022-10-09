// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_DEVICE_INSIDE) && !defined (VALENT_DEVICE_COMPILATION)
# error "Only <libvalent-device.h> can be included directly."
#endif

#include <gio/gio.h>
#include <json-glib/json-glib.h>
#include <libpeas/peas.h>
#include <libvalent-core.h>


G_BEGIN_DECLS

#define VALENT_TYPE_CHANNEL_SERVICE (valent_channel_service_get_type())

VALENT_AVAILABLE_IN_1_0
G_DECLARE_DERIVABLE_TYPE (ValentChannelService, valent_channel_service, VALENT, CHANNEL_SERVICE, ValentObject)

struct _ValentChannelServiceClass
{
  ValentObjectClass   parent_class;

  /* virtual functions */
  void                (*build_identity) (ValentChannelService  *service);
  void                (*identify)       (ValentChannelService  *service,
                                         const char            *target);
  void                (*start)          (ValentChannelService  *service,
                                         GCancellable          *cancellable,
                                         GAsyncReadyCallback    callback,
                                         gpointer               user_data);
  gboolean            (*start_finish)   (ValentChannelService  *service,
                                         GAsyncResult          *result,
                                         GError               **error);
  void                (*stop)           (ValentChannelService  *service);

  /* signals */
  void                (*channel)        (ValentChannelService  *service,
                                         ValentChannel         *channel);

  /*< private >*/
  gpointer            padding[8];
};

VALENT_AVAILABLE_IN_1_0
void         valent_channel_service_emit_channel    (ValentChannelService  *service,
                                                     ValentChannel         *channel);
VALENT_AVAILABLE_IN_1_0
char       * valent_channel_service_dup_id          (ValentChannelService  *service);
VALENT_AVAILABLE_IN_1_0
JsonNode   * valent_channel_service_ref_identity    (ValentChannelService  *service);
VALENT_AVAILABLE_IN_1_0
const char * valent_channel_service_get_name        (ValentChannelService  *service);
VALENT_AVAILABLE_IN_1_0
void         valent_channel_service_set_name        (ValentChannelService  *service,
                                                     const char            *name);
VALENT_AVAILABLE_IN_1_0
void         valent_channel_service_build_identity  (ValentChannelService  *service);
VALENT_AVAILABLE_IN_1_0
void         valent_channel_service_identify        (ValentChannelService  *service,
                                                     const char            *target);
VALENT_AVAILABLE_IN_1_0
void         valent_channel_service_start           (ValentChannelService  *service,
                                                     GCancellable          *cancellable,
                                                     GAsyncReadyCallback    callback,
                                                     gpointer               user_data);
VALENT_AVAILABLE_IN_1_0
gboolean     valent_channel_service_start_finish    (ValentChannelService  *service,
                                                     GAsyncResult          *result,
                                                     GError               **error);
VALENT_AVAILABLE_IN_1_0
void         valent_channel_service_stop            (ValentChannelService  *service);
VALENT_AVAILABLE_IN_1_0
gboolean     valent_channel_service_supports_plugin (ValentChannelService  *service,
                                                     PeasPluginInfo        *info);

G_END_DECLS

