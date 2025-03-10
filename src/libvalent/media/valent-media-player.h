// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_INSIDE) && !defined (VALENT_COMPILATION)
# error "Only <valent.h> can be included directly."
#endif

#include "../core/valent-resource.h"

G_BEGIN_DECLS

/**
 * ValentMediaActions:
 * @VALENT_MEDIA_ACTION_NONE: The player can not be controlled
 * @VALENT_MEDIA_ACTION_PLAY: Start or resume playback
 * @VALENT_MEDIA_ACTION_PAUSE: Pause playback
 * @VALENT_MEDIA_ACTION_STOP: Stop playback
 * @VALENT_MEDIA_ACTION_NEXT: Next track
 * @VALENT_MEDIA_ACTION_PREVIOUS: Previous track
 * @VALENT_MEDIA_ACTION_SEEK: Change the playback position
 * @VALENT_MEDIA_ACTION_RESERVED1: Reserved
 * @VALENT_MEDIA_ACTION_RESERVED2: Reserved
 *
 * Media action flags.
 *
 * Since: 1.0
 */
typedef enum
{
  VALENT_MEDIA_ACTION_NONE,
  VALENT_MEDIA_ACTION_PLAY      = (1<<0),
  VALENT_MEDIA_ACTION_PAUSE     = (1<<1),
  VALENT_MEDIA_ACTION_STOP      = (1<<2),
  VALENT_MEDIA_ACTION_NEXT      = (1<<3),
  VALENT_MEDIA_ACTION_PREVIOUS  = (1<<4),
  VALENT_MEDIA_ACTION_SEEK      = (1<<5),
  VALENT_MEDIA_ACTION_RESERVED1 = (1<<6),
  VALENT_MEDIA_ACTION_RESERVED2 = (1<<7)
} ValentMediaActions;


/**
 * ValentMediaRepeat:
 * @VALENT_MEDIA_REPEAT_NONE: Repeat off.
 * @VALENT_MEDIA_REPEAT_ALL: Repeat all items.
 * @VALENT_MEDIA_REPEAT_ONE: Repeat one items.
 *
 * Enumeration of loop modes.
 *
 * Since: 1.0
 */
typedef enum
{
  VALENT_MEDIA_REPEAT_NONE,
  VALENT_MEDIA_REPEAT_ALL,
  VALENT_MEDIA_REPEAT_ONE,
} ValentMediaRepeat;


/**
 * ValentMediaState:
 * @VALENT_MEDIA_STATE_STOPPED: The player state is unknown.
 * @VALENT_MEDIA_STATE_PLAYING: Playback is active.
 * @VALENT_MEDIA_STATE_PAUSED: Playback is paused.
 *
 * Media state flags.
 *
 * Since: 1.0
 */
typedef enum
{
  VALENT_MEDIA_STATE_STOPPED,
  VALENT_MEDIA_STATE_PLAYING,
  VALENT_MEDIA_STATE_PAUSED,
} ValentMediaState;


#define VALENT_TYPE_MEDIA_PLAYER (valent_media_player_get_type())

VALENT_AVAILABLE_IN_1_0
G_DECLARE_DERIVABLE_TYPE (ValentMediaPlayer, valent_media_player, VALENT, MEDIA_PLAYER, ValentResource)

struct _ValentMediaPlayerClass
{
  ValentResourceClass  parent_class;

  /* virtual functions */
  ValentMediaActions   (*get_flags)    (ValentMediaPlayer *player);
  GVariant           * (*get_metadata) (ValentMediaPlayer *player);
  const char         * (*get_name)     (ValentMediaPlayer *player);
  double               (*get_position) (ValentMediaPlayer *player);
  void                 (*set_position) (ValentMediaPlayer *player,
                                        double             position);
  ValentMediaRepeat    (*get_repeat)   (ValentMediaPlayer *player);
  void                 (*set_repeat)   (ValentMediaPlayer *player,
                                        ValentMediaRepeat  repeat);
  gboolean             (*get_shuffle)  (ValentMediaPlayer *player);
  void                 (*set_shuffle)  (ValentMediaPlayer *player,
                                        gboolean           shuffle);
  ValentMediaState     (*get_state)    (ValentMediaPlayer *player);
  double               (*get_volume)   (ValentMediaPlayer *player);
  void                 (*set_volume)   (ValentMediaPlayer *player,
                                        double             volume);
  void                 (*next)         (ValentMediaPlayer *player);
  void                 (*pause)        (ValentMediaPlayer *player);
  void                 (*play)         (ValentMediaPlayer *player);
  void                 (*previous)     (ValentMediaPlayer *player);
  void                 (*seek)         (ValentMediaPlayer *player,
                                        double             offset);
  void                 (*stop)         (ValentMediaPlayer *player);

  /*< private >*/
  gpointer             padding[8];
};

VALENT_AVAILABLE_IN_1_0
ValentMediaActions   valent_media_player_get_flags    (ValentMediaPlayer *player);
VALENT_AVAILABLE_IN_1_0
GVariant           * valent_media_player_get_metadata (ValentMediaPlayer *player);
VALENT_AVAILABLE_IN_1_0
const char         * valent_media_player_get_name     (ValentMediaPlayer *player);
VALENT_AVAILABLE_IN_1_0
double               valent_media_player_get_position (ValentMediaPlayer *player);
VALENT_AVAILABLE_IN_1_0
void                 valent_media_player_set_position (ValentMediaPlayer *player,
                                                       double             position);
VALENT_AVAILABLE_IN_1_0
ValentMediaRepeat    valent_media_player_get_repeat   (ValentMediaPlayer *player);
VALENT_AVAILABLE_IN_1_0
void                 valent_media_player_set_repeat   (ValentMediaPlayer *player,
                                                       ValentMediaRepeat  repeat);
VALENT_AVAILABLE_IN_1_0
gboolean             valent_media_player_get_shuffle  (ValentMediaPlayer *player);
VALENT_AVAILABLE_IN_1_0
void                 valent_media_player_set_shuffle  (ValentMediaPlayer *player,
                                                       gboolean           shuffle);
VALENT_AVAILABLE_IN_1_0
ValentMediaState     valent_media_player_get_state    (ValentMediaPlayer *player);
VALENT_AVAILABLE_IN_1_0
double               valent_media_player_get_volume   (ValentMediaPlayer *player);
VALENT_AVAILABLE_IN_1_0
void                 valent_media_player_set_volume   (ValentMediaPlayer *player,
                                                       double             volume);
VALENT_AVAILABLE_IN_1_0
void                 valent_media_player_next         (ValentMediaPlayer *player);
VALENT_AVAILABLE_IN_1_0
void                 valent_media_player_pause        (ValentMediaPlayer *player);
VALENT_AVAILABLE_IN_1_0
void                 valent_media_player_play         (ValentMediaPlayer *player);
VALENT_AVAILABLE_IN_1_0
void                 valent_media_player_previous     (ValentMediaPlayer *player);
VALENT_AVAILABLE_IN_1_0
void                 valent_media_player_seek         (ValentMediaPlayer *player,
                                                       double             offset);
VALENT_AVAILABLE_IN_1_0
void                 valent_media_player_stop         (ValentMediaPlayer *player);

G_END_DECLS

