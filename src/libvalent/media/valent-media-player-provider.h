// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_MEDIA_INSIDE) && !defined (VALENT_MEDIA_COMPILATION)
# error "Only <libvalent-media.h> can be included directly."
#endif

#include <gio/gio.h>

#include "valent-media-player.h"

G_BEGIN_DECLS

#define VALENT_TYPE_MEDIA_PLAYER_PROVIDER (valent_media_player_provider_get_type())

VALENT_AVAILABLE_IN_1_0
G_DECLARE_DERIVABLE_TYPE (ValentMediaPlayerProvider, valent_media_player_provider, VALENT, MEDIA_PLAYER_PROVIDER, GObject)

struct _ValentMediaPlayerProviderClass
{
  GObjectClass   parent_class;

  /* virtual functions */
  void           (*load_async)     (ValentMediaPlayerProvider  *provider,
                                    GCancellable               *cancellable,
                                    GAsyncReadyCallback         callback,
                                    gpointer                    user_data);
  gboolean       (*load_finish)    (ValentMediaPlayerProvider  *provider,
                                    GAsyncResult               *result,
                                    GError                    **error);

  /* signals */
  void           (*player_added)   (ValentMediaPlayerProvider  *provider,
                                    ValentMediaPlayer          *player);
  void           (*player_removed) (ValentMediaPlayerProvider  *provider,
                                    ValentMediaPlayer          *player);
};

VALENT_AVAILABLE_IN_1_0
void        valent_media_player_provider_emit_player_added   (ValentMediaPlayerProvider  *provider,
                                                              ValentMediaPlayer          *player);
VALENT_AVAILABLE_IN_1_0
void        valent_media_player_provider_emit_player_removed (ValentMediaPlayerProvider  *provider,
                                                              ValentMediaPlayer          *player);
VALENT_AVAILABLE_IN_1_0
GPtrArray * valent_media_player_provider_get_players         (ValentMediaPlayerProvider  *provider);
VALENT_AVAILABLE_IN_1_0
void        valent_media_player_provider_load_async          (ValentMediaPlayerProvider  *provider,
                                                              GCancellable               *cancellable,
                                                              GAsyncReadyCallback         callback,
                                                              gpointer                    user_data);
VALENT_AVAILABLE_IN_1_0
gboolean    valent_media_player_provider_load_finish         (ValentMediaPlayerProvider  *provider,
                                                              GAsyncResult               *result,
                                                              GError                    **error);

G_END_DECLS

