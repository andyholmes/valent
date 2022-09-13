// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-mpris-player"

#include "config.h"

#include <gio/gio.h>
#include <libvalent-media.h>

#include "valent-mock-media-player.h"


struct _ValentMockMediaPlayer
{
  ValentMediaPlayer  parent_instance;

  /* org.mpris.MediaPlayer2 */
  GVariant          *metadata;
  gint64             position;
  ValentMediaRepeat  repeat;
  gboolean           shuffle;
  ValentMediaState   state;
  double             volume;
};


G_DEFINE_TYPE (ValentMockMediaPlayer, valent_mock_media_player, VALENT_TYPE_MEDIA_PLAYER)


enum {
  PLAYER_METHOD,
  N_SIGNALS
};

static guint signals [N_SIGNALS] = { 0, };


/*
 * ValentMediaPlayer
 */
static GVariant *
valent_mock_media_player_get_metadata (ValentMediaPlayer *player)
{
  ValentMockMediaPlayer *self = VALENT_MOCK_MEDIA_PLAYER (player);

  if (self->metadata)
    return g_variant_ref (self->metadata);

  return NULL;
}

static gint64
valent_mock_media_player_get_position (ValentMediaPlayer *player)
{
  ValentMockMediaPlayer *self = VALENT_MOCK_MEDIA_PLAYER (player);

  return self->position;
}

static void
valent_mock_media_player_set_position (ValentMediaPlayer *player,
                                       gint64             position)
{
  ValentMockMediaPlayer *self = VALENT_MOCK_MEDIA_PLAYER (player);

  self->position = position;
}

static ValentMediaRepeat
valent_mock_media_player_get_repeat (ValentMediaPlayer *player)
{
  ValentMockMediaPlayer *self = VALENT_MOCK_MEDIA_PLAYER (player);

  return self->repeat;
}

static void
valent_mock_media_player_set_repeat (ValentMediaPlayer *player,
                                     ValentMediaRepeat  repeat)
{
  ValentMockMediaPlayer *self = VALENT_MOCK_MEDIA_PLAYER (player);

  self->repeat = repeat;
}

static gboolean
valent_mock_media_player_get_shuffle (ValentMediaPlayer *player)
{
  ValentMockMediaPlayer *self = VALENT_MOCK_MEDIA_PLAYER (player);

  return self->shuffle;
}

static void
valent_mock_media_player_set_shuffle (ValentMediaPlayer *player,
                                      gboolean           shuffle)
{
  ValentMockMediaPlayer *self = VALENT_MOCK_MEDIA_PLAYER (player);

  self->shuffle = shuffle;
}

static ValentMediaState
valent_mock_media_player_get_state (ValentMediaPlayer *player)
{
  ValentMockMediaPlayer *self = VALENT_MOCK_MEDIA_PLAYER (player);

  return self->state;
}

static double
valent_mock_media_player_get_volume (ValentMediaPlayer *player)
{
  ValentMockMediaPlayer *self = VALENT_MOCK_MEDIA_PLAYER (player);

  return self->volume;
}

static void
valent_mock_media_player_set_volume (ValentMediaPlayer *player,
                                     double             volume)
{
  ValentMockMediaPlayer *self = VALENT_MOCK_MEDIA_PLAYER (player);

  self->volume = volume;
}

static void
valent_mock_media_player_next (ValentMediaPlayer *player)
{
  ValentMockMediaPlayer *self = VALENT_MOCK_MEDIA_PLAYER (player);

  g_signal_emit (G_OBJECT (self), signals [PLAYER_METHOD], 0, "Next", NULL);
}

static void
valent_mock_media_player_pause (ValentMediaPlayer *player)
{
  ValentMockMediaPlayer *self = VALENT_MOCK_MEDIA_PLAYER (player);

  self->state = VALENT_MEDIA_STATE_PAUSED;
  g_signal_emit (G_OBJECT (self), signals [PLAYER_METHOD], 0, "Pause", NULL);
}

static void
valent_mock_media_player_play (ValentMediaPlayer *player)
{
  ValentMockMediaPlayer *self = VALENT_MOCK_MEDIA_PLAYER (player);

  self->state = VALENT_MEDIA_STATE_PLAYING;
  g_signal_emit (G_OBJECT (self), signals [PLAYER_METHOD], 0, "Play", NULL);
}

static void
valent_mock_media_player_play_pause (ValentMediaPlayer *player)
{
  ValentMockMediaPlayer *self = VALENT_MOCK_MEDIA_PLAYER (player);

  if (self->state == VALENT_MEDIA_STATE_PAUSED)
    self->state = VALENT_MEDIA_STATE_PLAYING;
  else if (self->state == VALENT_MEDIA_STATE_PLAYING)
    self->state = VALENT_MEDIA_STATE_PAUSED;

  g_signal_emit (G_OBJECT (self), signals [PLAYER_METHOD], 0, "PlayPause", NULL);
}

static void
valent_mock_media_player_previous (ValentMediaPlayer *player)
{
  ValentMockMediaPlayer *self = VALENT_MOCK_MEDIA_PLAYER (player);

  g_signal_emit (G_OBJECT (self), signals [PLAYER_METHOD], 0, "Previous", NULL);
}

static void
valent_mock_media_player_seek (ValentMediaPlayer *player,
                               gint64             offset)
{
  ValentMockMediaPlayer *self = VALENT_MOCK_MEDIA_PLAYER (player);

  g_signal_emit (G_OBJECT (self), signals [PLAYER_METHOD], 0, "Seek", g_variant_new_int64 (offset));
}

static void
valent_mock_media_player_stop (ValentMediaPlayer *player)
{
  ValentMockMediaPlayer *self = VALENT_MOCK_MEDIA_PLAYER (player);

  g_signal_emit (G_OBJECT (self), signals [PLAYER_METHOD], 0, "Stop", NULL);
}

/*
 * GObject
 */
static void
valent_mock_media_player_finalize (GObject *object)
{
  ValentMockMediaPlayer *self = VALENT_MOCK_MEDIA_PLAYER (object);

  g_clear_pointer (&self->metadata, g_variant_unref);

  G_OBJECT_CLASS (valent_mock_media_player_parent_class)->finalize (object);
}

static void
valent_mock_media_player_class_init (ValentMockMediaPlayerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentMediaPlayerClass *player_class = VALENT_MEDIA_PLAYER_CLASS (klass);

  object_class->finalize = valent_mock_media_player_finalize;

  player_class->get_metadata = valent_mock_media_player_get_metadata;
  player_class->get_position = valent_mock_media_player_get_position;
  player_class->set_position = valent_mock_media_player_set_position;
  player_class->get_repeat = valent_mock_media_player_get_repeat;
  player_class->set_repeat = valent_mock_media_player_set_repeat;
  player_class->get_shuffle = valent_mock_media_player_get_shuffle;
  player_class->set_shuffle = valent_mock_media_player_set_shuffle;
  player_class->get_state = valent_mock_media_player_get_state;
  player_class->get_volume = valent_mock_media_player_get_volume;
  player_class->set_volume = valent_mock_media_player_set_volume;

  player_class->next = valent_mock_media_player_next;
  player_class->pause = valent_mock_media_player_pause;
  player_class->play = valent_mock_media_player_play;
  player_class->play_pause = valent_mock_media_player_play_pause;
  player_class->previous = valent_mock_media_player_previous;
  player_class->seek = valent_mock_media_player_seek;
  player_class->stop = valent_mock_media_player_stop;

  /**
   * ValentMockMediaPlayer::player-method:
   * @player: a #ValentMprisRemote
   * @method_name: the method name
   * @method_args: the method arguments
   *
   * #ValentMprisRemote::player-method is emitted when a method is called by a
   * consumer of the exported interface.
   */
  signals [PLAYER_METHOD] =
    g_signal_new ("player-method",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_VARIANT);
}

static void
valent_mock_media_player_init (ValentMockMediaPlayer *self)
{
  GVariantDict dict;

  g_variant_dict_init (&dict, NULL);
  self->metadata = g_variant_dict_end (&dict);
  g_variant_ref_sink (self->metadata);
}

