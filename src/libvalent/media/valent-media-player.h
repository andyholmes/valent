// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_MEDIA_INSIDE) && !defined (VALENT_MEDIA_COMPILATION)
# error "Only <libvalent-media.h> can be included directly."
#endif

#include <libvalent-core.h>

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
 * ValentMediaState:
 * @VALENT_MEDIA_STATE_UNKNOWN: The player state is unknown.
 * @VALENT_MEDIA_STATE_PLAYING: Playback is active.
 * @VALENT_MEDIA_STATE_PAUSED: Playback is paused.
 * @VALENT_MEDIA_STATE_STOPPED: Playback is halted.
 * @VALENT_MEDIA_STATE_REPEAT: The current item will be restarted when it finishes.
 * @VALENT_MEDIA_STATE_REPEAT_ALL: The item queue will be restarted when it finishes.
 * @VALENT_MEDIA_STATE_SHUFFLE: Playback order is non-linear.
 * @VALENT_MEDIA_STATE_RESERVED1: Reserved
 * @VALENT_MEDIA_STATE_RESERVED2: Reserved
 * @VALENT_MEDIA_STATE_RESERVED3: Reserved
 *
 * Media state flags.
 *
 * Since: 1.0
 */
typedef enum
{
  VALENT_MEDIA_STATE_STOPPED,
  VALENT_MEDIA_STATE_PLAYING    = (1<<0),
  VALENT_MEDIA_STATE_PAUSED     = (1<<1),
  VALENT_MEDIA_STATE_REPEAT     = (1<<2),
  VALENT_MEDIA_STATE_REPEAT_ALL = (1<<3),
  VALENT_MEDIA_STATE_SHUFFLE    = (1<<4),
  VALENT_MEDIA_STATE_RESERVED1  = (1<<5),
  VALENT_MEDIA_STATE_RESERVED2  = (1<<6),
  VALENT_MEDIA_STATE_RESERVED3  = (1<<7)
} ValentMediaState;


#define VALENT_TYPE_MEDIA_PLAYER (valent_media_player_get_type())

VALENT_AVAILABLE_IN_1_0
G_DECLARE_DERIVABLE_TYPE (ValentMediaPlayer, valent_media_player, VALENT, MEDIA_PLAYER, GObject)

struct _ValentMediaPlayerClass
{
  GObjectClass         parent_class;

  /* virtual functions */
  void                 (*next)         (ValentMediaPlayer *player);
  void                 (*open_uri)     (ValentMediaPlayer *player,
                                        const char        *uri);
  void                 (*pause)        (ValentMediaPlayer *player);
  void                 (*play)         (ValentMediaPlayer *player);
  void                 (*play_pause)   (ValentMediaPlayer *player);
  void                 (*previous)     (ValentMediaPlayer *player);
  void                 (*seek)         (ValentMediaPlayer *player,
                                        gint64             offset);
  void                 (*stop)         (ValentMediaPlayer *player);

  ValentMediaActions   (*get_flags)    (ValentMediaPlayer *player);
  GVariant           * (*get_metadata) (ValentMediaPlayer *player);
  const char         * (*get_name)     (ValentMediaPlayer *player);
  gint64               (*get_position) (ValentMediaPlayer *player);
  void                 (*set_position) (ValentMediaPlayer *player,
                                        const char        *track_id,
                                        gint64             position);
  ValentMediaState     (*get_state)    (ValentMediaPlayer *player);
  void                 (*set_state)    (ValentMediaPlayer *player,
                                        ValentMediaState   state);
  double               (*get_volume)   (ValentMediaPlayer *player);
  void                 (*set_volume)   (ValentMediaPlayer *player,
                                        double             volume);

  /* signals */
  void                 (*changed)      (ValentMediaPlayer *player);
  void                 (*seeked)       (ValentMediaPlayer *player,
                                        gint64             position);

  /*< private >*/
  gpointer             padding[8];
};

VALENT_AVAILABLE_IN_1_0
void                 valent_media_player_emit_changed (ValentMediaPlayer *player);
VALENT_AVAILABLE_IN_1_0
void                 valent_media_player_emit_seeked  (ValentMediaPlayer *player,
                                                       gint64             offset);
VALENT_AVAILABLE_IN_1_0
gboolean             valent_media_player_is_playing   (ValentMediaPlayer *player);
VALENT_AVAILABLE_IN_1_0
ValentMediaActions   valent_media_player_get_flags    (ValentMediaPlayer *player);
VALENT_AVAILABLE_IN_1_0
GVariant           * valent_media_player_get_metadata (ValentMediaPlayer *player);
VALENT_AVAILABLE_IN_1_0
const char         * valent_media_player_get_name     (ValentMediaPlayer *player);
VALENT_AVAILABLE_IN_1_0
gint64               valent_media_player_get_position (ValentMediaPlayer *player);
VALENT_AVAILABLE_IN_1_0
void                 valent_media_player_set_position (ValentMediaPlayer *player,
                                                       const char        *track_id,
                                                       gint64             position);
VALENT_AVAILABLE_IN_1_0
ValentMediaState     valent_media_player_get_state    (ValentMediaPlayer *player);
VALENT_AVAILABLE_IN_1_0
void                 valent_media_player_set_state    (ValentMediaPlayer *player,
                                                       ValentMediaState   state);
VALENT_AVAILABLE_IN_1_0
double               valent_media_player_get_volume   (ValentMediaPlayer *player);
VALENT_AVAILABLE_IN_1_0
void                 valent_media_player_set_volume   (ValentMediaPlayer *player,
                                                       double             volume);

VALENT_AVAILABLE_IN_1_0
void                 valent_media_player_next         (ValentMediaPlayer *player);
VALENT_AVAILABLE_IN_1_0
void                 valent_media_player_open_uri     (ValentMediaPlayer *player,
                                                       const char        *uri);
VALENT_AVAILABLE_IN_1_0
void                 valent_media_player_pause        (ValentMediaPlayer *player);
VALENT_AVAILABLE_IN_1_0
void                 valent_media_player_play         (ValentMediaPlayer *player);
VALENT_AVAILABLE_IN_1_0
void                 valent_media_player_play_pause   (ValentMediaPlayer *player);
VALENT_AVAILABLE_IN_1_0
void                 valent_media_player_previous     (ValentMediaPlayer *player);
VALENT_AVAILABLE_IN_1_0
void                 valent_media_player_seek         (ValentMediaPlayer *player,
                                                       gint64             offset);
VALENT_AVAILABLE_IN_1_0
void                 valent_media_player_stop         (ValentMediaPlayer *player);

G_END_DECLS

