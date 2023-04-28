// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_INSIDE) && !defined (VALENT_COMPILATION)
# error "Only <valent.h> can be included directly."
#endif

#include "../core/valent-extension.h"
#include "valent-media-player.h"

G_BEGIN_DECLS

#define VALENT_TYPE_MEDIA_ADAPTER (valent_media_adapter_get_type())

VALENT_AVAILABLE_IN_1_0
G_DECLARE_DERIVABLE_TYPE (ValentMediaAdapter, valent_media_adapter, VALENT, MEDIA_ADAPTER, ValentExtension)

struct _ValentMediaAdapterClass
{
  ValentExtensionClass   parent_class;

  /* virtual functions */
  void                   (*export_player)   (ValentMediaAdapter *adapter,
                                             ValentMediaPlayer  *player);
  void                   (*unexport_player) (ValentMediaAdapter *adapter,
                                             ValentMediaPlayer  *player);

  /*< private >*/
  gpointer               padding[8];
};

VALENT_AVAILABLE_IN_1_0
void   valent_media_adapter_player_added    (ValentMediaAdapter *adapter,
                                             ValentMediaPlayer  *player);
VALENT_AVAILABLE_IN_1_0
void   valent_media_adapter_player_removed  (ValentMediaAdapter *adapter,
                                             ValentMediaPlayer  *player);
VALENT_AVAILABLE_IN_1_0
void   valent_media_adapter_export_player   (ValentMediaAdapter *adapter,
                                             ValentMediaPlayer  *player);
VALENT_AVAILABLE_IN_1_0
void   valent_media_adapter_unexport_player (ValentMediaAdapter *adapter,
                                             ValentMediaPlayer  *player);

G_END_DECLS

