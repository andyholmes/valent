// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_MEDIA_INSIDE) && !defined (VALENT_MEDIA_COMPILATION)
# error "Only <libvalent-media.h> can be included directly."
#endif

#include <gio/gio.h>

#include "valent-media-player.h"

G_BEGIN_DECLS

#define VALENT_TYPE_MEDIA_ADAPTER (valent_media_adapter_get_type())

VALENT_AVAILABLE_IN_1_0
G_DECLARE_DERIVABLE_TYPE (ValentMediaAdapter, valent_media_adapter, VALENT, MEDIA_ADAPTER, GObject)

struct _ValentMediaAdapterClass
{
  GObjectClass   parent_class;

  /* virtual functions */
  void           (*load_async)     (ValentMediaAdapter   *adapter,
                                    GCancellable         *cancellable,
                                    GAsyncReadyCallback   callback,
                                    gpointer              user_data);
  gboolean       (*load_finish)    (ValentMediaAdapter   *adapter,
                                    GAsyncResult         *result,
                                    GError              **error);

  /* signals */
  void           (*player_added)   (ValentMediaAdapter   *adapter,
                                    ValentMediaPlayer    *player);
  void           (*player_removed) (ValentMediaAdapter   *adapter,
                                    ValentMediaPlayer    *player);

  /*< private >*/
  gpointer       padding[8];
};

VALENT_AVAILABLE_IN_1_0
void        valent_media_adapter_emit_player_added   (ValentMediaAdapter   *adapter,
                                                      ValentMediaPlayer    *player);
VALENT_AVAILABLE_IN_1_0
void        valent_media_adapter_emit_player_removed (ValentMediaAdapter   *adapter,
                                                      ValentMediaPlayer    *player);
VALENT_AVAILABLE_IN_1_0
GPtrArray * valent_media_adapter_get_players         (ValentMediaAdapter   *adapter);
VALENT_AVAILABLE_IN_1_0
void        valent_media_adapter_load_async          (ValentMediaAdapter   *adapter,
                                                      GCancellable         *cancellable,
                                                      GAsyncReadyCallback   callback,
                                                      gpointer              user_data);
VALENT_AVAILABLE_IN_1_0
gboolean    valent_media_adapter_load_finish         (ValentMediaAdapter   *adapter,
                                                      GAsyncResult         *result,
                                                      GError              **error);

G_END_DECLS

