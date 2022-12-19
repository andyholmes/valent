// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-mpris-player"

#include "config.h"

#include <gio/gio.h>
#include <libvalent-media.h>

#include "valent-mock-media-player.h"


struct _ValentMockMediaPlayer
{
  ValentMediaPlayer   parent_instance;

  ValentMediaActions  flags;
  GVariant           *metadata;
  double              position;
  ValentMediaRepeat   repeat;
  gboolean            shuffle;
  ValentMediaState    state;
  double              volume;

  unsigned int        track;
};


G_DEFINE_TYPE (ValentMockMediaPlayer, valent_mock_media_player, VALENT_TYPE_MEDIA_PLAYER)


static const char *tracks[] = {
  "{'xesam:title': <'Track 1'>, 'xesam:album': <'Test Album'>, 'xesam:artist': <['Test Artist']>, 'mpris:length': <int64 180000000>}",
  "{'xesam:title': <'Track 2'>, 'xesam:album': <'Test Album'>, 'xesam:artist': <['Test Artist']>, 'mpris:length': <int64 180000000>}",
  "{'xesam:title': <'Track 3'>, 'xesam:album': <'Test Album'>, 'xesam:artist': <['Test Artist']>, 'mpris:length': <int64 180000000>}",
};


static void
valent_mock_media_player_update_state (ValentMockMediaPlayer *self)
{
  g_assert (VALENT_IS_MOCK_MEDIA_PLAYER (self));

  g_clear_pointer (&self->metadata, g_variant_unref);
  self->metadata = g_variant_parse (NULL, tracks[self->track], NULL, NULL, NULL);

  if (self->track == 0)
    {
      self->flags |= VALENT_MEDIA_ACTION_NEXT;
      self->flags &= ~VALENT_MEDIA_ACTION_PREVIOUS;
    }
  else if (self->track == (G_N_ELEMENTS (tracks) - 1))
    {
      self->flags &= ~VALENT_MEDIA_ACTION_NEXT;
      self->flags |= VALENT_MEDIA_ACTION_PREVIOUS;
    }
  else
    {
      self->flags |= VALENT_MEDIA_ACTION_NEXT;
      self->flags |= VALENT_MEDIA_ACTION_PREVIOUS;
    }

  if (self->state == VALENT_MEDIA_STATE_PAUSED)
    {
      self->flags &= ~VALENT_MEDIA_ACTION_PAUSE;
      self->flags |= VALENT_MEDIA_ACTION_PLAY;
      self->flags |= VALENT_MEDIA_ACTION_SEEK;
      self->flags |= VALENT_MEDIA_ACTION_STOP;
    }
  else if (self->state == VALENT_MEDIA_STATE_PLAYING)
    {
      self->flags |= VALENT_MEDIA_ACTION_PAUSE;
      self->flags &= ~VALENT_MEDIA_ACTION_PLAY;
      self->flags |= VALENT_MEDIA_ACTION_SEEK;
      self->flags |= VALENT_MEDIA_ACTION_STOP;
    }
  else if (self->state == VALENT_MEDIA_STATE_STOPPED)
    {
      self->flags &= ~VALENT_MEDIA_ACTION_NEXT;
      self->flags &= ~VALENT_MEDIA_ACTION_PAUSE;
      self->flags |= VALENT_MEDIA_ACTION_PLAY;
      self->flags &= ~VALENT_MEDIA_ACTION_PREVIOUS;
      self->flags &= ~VALENT_MEDIA_ACTION_SEEK;
      self->flags &= ~VALENT_MEDIA_ACTION_STOP;

      g_clear_pointer (&self->metadata, g_variant_unref);
    }

  g_object_notify (G_OBJECT (self), "flags");
  g_object_notify (G_OBJECT (self), "metadata");
  g_object_notify (G_OBJECT (self), "state");
}


/*
 * ValentMediaPlayer
 */
static ValentMediaActions
valent_mock_media_player_get_flags (ValentMediaPlayer *player)
{
  ValentMockMediaPlayer *self = VALENT_MOCK_MEDIA_PLAYER (player);

  return self->flags;
}

static GVariant *
valent_mock_media_player_get_metadata (ValentMediaPlayer *player)
{
  ValentMockMediaPlayer *self = VALENT_MOCK_MEDIA_PLAYER (player);

  if (self->metadata)
    return g_variant_ref (self->metadata);

  return NULL;
}

static const char *
valent_mock_media_player_get_name (ValentMediaPlayer *player)
{
  return "Mock Player";
}

static double
valent_mock_media_player_get_position (ValentMediaPlayer *player)
{
  ValentMockMediaPlayer *self = VALENT_MOCK_MEDIA_PLAYER (player);

  return self->position;
}

static void
valent_mock_media_player_set_position (ValentMediaPlayer *player,
                                       double             position)
{
  ValentMockMediaPlayer *self = VALENT_MOCK_MEDIA_PLAYER (player);

  self->position = position;
  g_object_notify (G_OBJECT (player), "position");
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
  g_object_notify (G_OBJECT (self), "repeat");
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
  g_object_notify (G_OBJECT (self), "shuffle");
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
  g_object_notify (G_OBJECT (self), "volume");
}

static void
valent_mock_media_player_next (ValentMediaPlayer *player)
{
  ValentMockMediaPlayer *self = VALENT_MOCK_MEDIA_PLAYER (player);

  self->track += 1;
  valent_mock_media_player_update_state (self);
}

static void
valent_mock_media_player_pause (ValentMediaPlayer *player)
{
  ValentMockMediaPlayer *self = VALENT_MOCK_MEDIA_PLAYER (player);

  self->state = VALENT_MEDIA_STATE_PAUSED;
  valent_mock_media_player_update_state (self);
  g_object_notify (G_OBJECT (self), "state");
}

static void
valent_mock_media_player_play (ValentMediaPlayer *player)
{
  ValentMockMediaPlayer *self = VALENT_MOCK_MEDIA_PLAYER (player);

  self->state = VALENT_MEDIA_STATE_PLAYING;
  valent_mock_media_player_update_state (self);
  g_object_notify (G_OBJECT (self), "state");
}

static void
valent_mock_media_player_play_pause (ValentMediaPlayer *player)
{
  ValentMockMediaPlayer *self = VALENT_MOCK_MEDIA_PLAYER (player);

  if (self->state == VALENT_MEDIA_STATE_PAUSED)
    valent_mock_media_player_play (player);
  else if (self->state == VALENT_MEDIA_STATE_PLAYING)
    valent_mock_media_player_pause (player);
}

static void
valent_mock_media_player_previous (ValentMediaPlayer *player)
{
  ValentMockMediaPlayer *self = VALENT_MOCK_MEDIA_PLAYER (player);

  self->track -= 1;
  valent_mock_media_player_update_state (self);
}

static void
valent_mock_media_player_seek (ValentMediaPlayer *player,
                               double             offset)
{
  ValentMockMediaPlayer *self = VALENT_MOCK_MEDIA_PLAYER (player);

  self->position += offset;
  g_object_notify (G_OBJECT (player), "position");
}

static void
valent_mock_media_player_stop (ValentMediaPlayer *player)
{
  ValentMockMediaPlayer *self = VALENT_MOCK_MEDIA_PLAYER (player);

  self->state = VALENT_MEDIA_STATE_STOPPED;
  valent_mock_media_player_update_state (self);
  g_object_notify (G_OBJECT (self), "state");
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

  player_class->get_flags = valent_mock_media_player_get_flags;
  player_class->get_name = valent_mock_media_player_get_name;
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
}

static void
valent_mock_media_player_init (ValentMockMediaPlayer *self)
{
  GVariantDict dict;

  g_variant_dict_init (&dict, NULL);
  self->metadata = g_variant_ref_sink (g_variant_dict_end (&dict));
  self->volume = 1.0;
}

/**
 * valent_mock_media_player_update_art:
 * @self: a #ValentMockMediaPlayer
 * @url: a URI
 *
 * Update the track metadata with album art at @uri.
 *
 * This is a convenience for unit tests, where the file location may not be
 * known until after compile-time.
 */
void
valent_mock_media_player_update_art (ValentMockMediaPlayer *self,
                                     const char            *uri)
{
  GVariantDict dict;
  g_autoptr (GVariant) metadata = NULL;

  metadata = g_steal_pointer (&self->metadata);

  g_variant_dict_init (&dict, metadata);
  g_variant_dict_insert (&dict, "mpris:artUrl", "s", uri);
  self->metadata = g_variant_ref_sink (g_variant_dict_end (&dict));
  g_object_notify (G_OBJECT (self), "metadata");
}

/**
 * valent_mock_media_player_update_flags:
 * @self: a #ValentMockMediaPlayer
 * @flags: a #ValentMediaActions
 *
 * Update the track metadata with album art at @uri.
 *
 * This is a convenience for unit tests, where the player must being with
 * particular flags set.
 */
void
valent_mock_media_player_update_flags (ValentMockMediaPlayer *self,
                                       ValentMediaActions     flags)
{
  self->flags = flags;
  g_object_notify (G_OBJECT (self), "flags");
}

