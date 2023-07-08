// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_INSIDE) && !defined (VALENT_COMPILATION)
# error "Only <valent.h> can be included directly."
#endif

#include "../core/valent-extension.h"
#include "valent-channel.h"

G_BEGIN_DECLS

#define VALENT_TYPE_CHANNEL_SERVICE (valent_channel_service_get_type())

VALENT_AVAILABLE_IN_1_0
G_DECLARE_DERIVABLE_TYPE (ValentChannelService, valent_channel_service, VALENT, CHANNEL_SERVICE, ValentExtension)

struct _ValentChannelServiceClass
{
  ValentExtensionClass   parent_class;

  /* virtual functions */
  void                   (*build_identity) (ValentChannelService  *service);
  void                   (*identify)       (ValentChannelService  *service,
                                            const char            *target);

  /* signals */
  void                   (*channel)        (ValentChannelService  *service,
                                            ValentChannel         *channel);

  /*< private >*/
  gpointer               padding[8];
};

VALENT_AVAILABLE_IN_1_0
void              valent_channel_service_channel         (ValentChannelService *service,
                                                          ValentChannel        *channel);
VALENT_AVAILABLE_IN_1_0
GTlsCertificate * valent_channel_service_ref_certificate (ValentChannelService *self);
VALENT_AVAILABLE_IN_1_0
char            * valent_channel_service_dup_id          (ValentChannelService *service);
VALENT_AVAILABLE_IN_1_0
JsonNode        * valent_channel_service_ref_identity    (ValentChannelService *service);
VALENT_AVAILABLE_IN_1_0
const char      * valent_channel_service_get_name        (ValentChannelService *service);
VALENT_AVAILABLE_IN_1_0
void              valent_channel_service_set_name        (ValentChannelService *service,
                                                          const char           *name);
VALENT_AVAILABLE_IN_1_0
void              valent_channel_service_build_identity  (ValentChannelService *service);
VALENT_AVAILABLE_IN_1_0
void              valent_channel_service_identify        (ValentChannelService *service,
                                                          const char           *target);

G_END_DECLS

