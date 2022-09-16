// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-media-player"

#include "config.h"

#include <gio/gio.h>

#include "valent-media-enums.h"
#include "valent-media-player.h"


/**
 * ValentMediaPlayer:
 *
 * A base class for media players.
 *
 * A #ValentMediaPlayer is a base class for plugins to providing an interface to
 * media players via [class@Valent.MediaAdapter].
 *
 * Since: 1.0
 */

G_DEFINE_TYPE (ValentMediaPlayer, valent_media_player, G_TYPE_OBJECT)

/**
 * ValentMediaPlayerClass:
 * @changed: the class closure for #ValentMediaPlayer::changed signal
 * @next: the virtual function pointer for valent_media_player_next()
 * @pause: the virtual function pointer for valent_media_player_pause()
 * @play: the virtual function pointer for valent_media_player_play()
 * @play_pause: the virtual function pointer for valent_media_player_play_pause()
 * @previous: the virtual function pointer for valent_media_player_previous()
 * @seek: the virtual function pointer for valent_media_player_seek()
 * @stop: the virtual function pointer for valent_media_player_stop()
 * @get_flags: Getter for the #ValentMediaPlayer:flags property.
 * @get_metadata: Getter for the #ValentMediaPlayer:metadata property.
 * @get_name: Getter for the #ValentMediaPlayer:name property.
 * @get_position: Getter for the #ValentMediaPlayer:position property.
 * @set_position: Setter for the #ValentMediaPlayer:position property.
 * @get_repeat: Getter for the #ValentMediaPlayer:repeat property.
 * @set_repeat: Setter for the #ValentMediaPlayer:repeat property.
 * @seeked: the class closure for the #ValentMediaPlayer::seeked signal
 * @get_state: Getter for the #ValentMediaPlayer:state property.
 * @get_volume: Getter for the #ValentMediaPlayer:volume property.
 * @set_volume: Setter for the #ValentMediaPlayer:volume property.
 *
 * Virtual table for #ValentMediaPlayer
 */

enum {
  PROP_0,
  PROP_FLAGS,
  PROP_METADATA,
  PROP_NAME,
  PROP_POSITION,
  PROP_REPEAT,
  PROP_SHUFFLE,
  PROP_STATE,
  PROP_VOLUME,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

enum {
  CHANGED,
  SEEKED,
  N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0, };


/* LCOV_EXCL_START */
static ValentMediaActions
valent_media_player_real_get_flags (ValentMediaPlayer *player)
{
  return VALENT_MEDIA_ACTION_NONE;
}

static GVariant *
valent_media_player_real_get_metadata (ValentMediaPlayer *player)
{
  return NULL;
}

static const char *
valent_media_player_real_get_name (ValentMediaPlayer *player)
{
  return "Media Player";
}

static gint64
valent_media_player_real_get_position (ValentMediaPlayer *player)
{
  return 0;
}

static void
valent_media_player_real_set_position (ValentMediaPlayer *player,
                                       gint64             position)
{
}

static ValentMediaRepeat
valent_media_player_real_get_repeat (ValentMediaPlayer *player)
{
  return VALENT_MEDIA_REPEAT_NONE;
}

static void
valent_media_player_real_set_repeat (ValentMediaPlayer *player,
                                     ValentMediaRepeat  repeat)
{
}

static gboolean
valent_media_player_real_get_shuffle (ValentMediaPlayer *player)
{
  return FALSE;
}

static void
valent_media_player_real_set_shuffle (ValentMediaPlayer *player,
                                      gboolean           shuffle)
{
}

static ValentMediaState
valent_media_player_real_get_state (ValentMediaPlayer *player)
{
  return VALENT_MEDIA_STATE_STOPPED;
}

static double
valent_media_player_real_get_volume (ValentMediaPlayer *player)
{
  return 1.0;
}

static void
valent_media_player_real_set_volume (ValentMediaPlayer *player,
                                     double             volume)
{
}

static void
valent_media_player_real_next (ValentMediaPlayer *player)
{
}

static void
valent_media_player_real_pause (ValentMediaPlayer *player)
{
}

static void
valent_media_player_real_play (ValentMediaPlayer *player)
{
}

static void
valent_media_player_real_play_pause (ValentMediaPlayer *player)
{
  ValentMediaActions flags = valent_media_player_get_flags (player);
  ValentMediaState state = valent_media_player_get_state (player);

  if (state == VALENT_MEDIA_STATE_PLAYING &&
      (flags & VALENT_MEDIA_ACTION_PAUSE) != 0)
    valent_media_player_pause (player);

  else if (state != VALENT_MEDIA_STATE_PLAYING &&
           (flags & VALENT_MEDIA_ACTION_PLAY) != 0)
    valent_media_player_play (player);
}

static void
valent_media_player_real_previous (ValentMediaPlayer *player)
{
  g_debug ("%s(): operation not supported", G_STRFUNC);
}

static void
valent_media_player_real_seek (ValentMediaPlayer *player,
                               gint64             offset)
{
  g_debug ("%s(): operation not supported", G_STRFUNC);
}

static void
valent_media_player_real_stop (ValentMediaPlayer *player)
{
  g_debug ("%s(): operation not supported", G_STRFUNC);
}
/* LCOV_EXCL_STOP */


/*
 * GObject
 */
static void
valent_media_player_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  ValentMediaPlayer *self = VALENT_MEDIA_PLAYER (object);

  switch (prop_id)
    {
    case PROP_FLAGS:
      g_value_set_flags (value, valent_media_player_get_flags (self));
      break;

    case PROP_METADATA:
      g_value_take_variant (value, valent_media_player_get_metadata (self));
      break;

    case PROP_NAME:
      g_value_set_string (value, valent_media_player_get_name (self));
      break;

    case PROP_POSITION:
      g_value_set_int64 (value, valent_media_player_get_position (self));
      break;

    case PROP_REPEAT:
      g_value_set_enum (value, valent_media_player_get_repeat (self));
      break;

    case PROP_SHUFFLE:
      g_value_set_boolean (value, valent_media_player_get_shuffle (self));
      break;

    case PROP_STATE:
      g_value_set_enum (value, valent_media_player_get_state (self));
      break;

    case PROP_VOLUME:
      g_value_set_double (value, valent_media_player_get_volume (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_media_player_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  ValentMediaPlayer *self = VALENT_MEDIA_PLAYER (object);

  switch (prop_id)
    {
    case PROP_POSITION:
      valent_media_player_set_position (self, g_value_get_int64 (value));
      break;

    case PROP_REPEAT:
      valent_media_player_set_repeat (self, g_value_get_enum (value));
      break;

    case PROP_SHUFFLE:
      valent_media_player_set_shuffle (self, g_value_get_boolean (value));
      break;

    case PROP_VOLUME:
      valent_media_player_set_volume (self, g_value_get_double (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_media_player_class_init (ValentMediaPlayerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentMediaPlayerClass *player_class = VALENT_MEDIA_PLAYER_CLASS (klass);

  object_class->get_property = valent_media_player_get_property;
  object_class->set_property = valent_media_player_set_property;

  player_class->get_flags = valent_media_player_real_get_flags;
  player_class->get_metadata = valent_media_player_real_get_metadata;
  player_class->get_name = valent_media_player_real_get_name;
  player_class->get_position = valent_media_player_real_get_position;
  player_class->set_position = valent_media_player_real_set_position;
  player_class->get_repeat = valent_media_player_real_get_repeat;
  player_class->set_repeat = valent_media_player_real_set_repeat;
  player_class->get_shuffle = valent_media_player_real_get_shuffle;
  player_class->set_shuffle = valent_media_player_real_set_shuffle;
  player_class->get_state = valent_media_player_real_get_state;
  player_class->get_volume = valent_media_player_real_get_volume;
  player_class->set_volume = valent_media_player_real_set_volume;
  player_class->next = valent_media_player_real_next;
  player_class->pause = valent_media_player_real_pause;
  player_class->play = valent_media_player_real_play;
  player_class->play_pause = valent_media_player_real_play_pause;
  player_class->previous = valent_media_player_real_previous;
  player_class->seek = valent_media_player_real_seek;
  player_class->stop = valent_media_player_real_stop;

  /**
   * ValentMediaPlayer:flags:
   *
   * The available actions.
   *
   * Implementations should emit [GObject.Object::notify] when they change the
   * internal representation of this property.
   *
   * Since: 1.0
   */
  properties [PROP_FLAGS] =
    g_param_spec_flags ("flags", NULL, NULL,
                        VALENT_TYPE_MEDIA_ACTIONS,
                        VALENT_MEDIA_ACTION_NONE,
                        (G_PARAM_READABLE |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  /**
   * ValentMediaPlayer:metadata: (getter get_metadata)
   *
   * The metadata of the active media item.
   *
   * The content of the vardict should be in the same format as the MPRISv2
   * standard.
   *
   * Implementations should emit [GObject.Object::notify] when they change the
   * internal representation of this property.
   *
   * Since: 1.0
   */
  properties [PROP_METADATA] =
    g_param_spec_variant ("metadata", NULL, NULL,
                          G_VARIANT_TYPE ("a{sv}"),
                          NULL,
                          (G_PARAM_READABLE |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  /**
   * ValentMediaPlayer:name: (getter get_name)
   *
   * The display name of the media player.
   *
   * Since: 1.0
   */
  properties [PROP_NAME] =
    g_param_spec_string ("name", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentMediaPlayer:position: (getter get_position) (setter set_position)
   *
   * The current track position in microseconds (us).
   *
   * Acceptable values are between `0` and the `mpris:length` metadata entry
   * (see [property@Valent.MediaPlayer:metadata]). If the player does not have
   * %VALENT_MEDIA_ACTION_SEEK in [property@Valent.MediaPlayer:flags], setting
   * this property should have no effect.
   *
   * Since: 1.0
   */
  properties [PROP_POSITION] =
    g_param_spec_int64 ("position", NULL, NULL,
                        G_MININT64, G_MAXINT64,
                        0,
                        (G_PARAM_READWRITE |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  /**
   * ValentMediaPlayer:repeat: (getter get_repeat) (setter set_repeat)
   *
   * The repeat mode.
   *
   * If the player does not have the appropriate bitmask in
   * [property@Valent.MediaPlayer:flags], setting this property should have no
   * effect.
   *
   * Since: 1.0
   */
  properties [PROP_REPEAT] =
    g_param_spec_enum ("repeat", NULL, NULL,
                       VALENT_TYPE_MEDIA_REPEAT,
                       VALENT_MEDIA_REPEAT_NONE,
                       (G_PARAM_READWRITE |
                        G_PARAM_EXPLICIT_NOTIFY |
                        G_PARAM_STATIC_STRINGS));

  /**
   * ValentMediaPlayer:state: (getter get_state)
   *
   * The playback state.
   *
   * If the player does not have the appropriate bitmask in
   * [property@Valent.MediaPlayer:flags], setting this property should have no
   * effect.
   *
   * Since: 1.0
   */
  properties [PROP_STATE] =
    g_param_spec_enum ("state", NULL, NULL,
                       VALENT_TYPE_MEDIA_STATE,
                       VALENT_MEDIA_STATE_STOPPED,
                       (G_PARAM_READWRITE |
                        G_PARAM_EXPLICIT_NOTIFY |
                        G_PARAM_STATIC_STRINGS));

  /**
   * ValentMediaPlayer:shuffle: (getter get_shuffle) (setter set_shuffle)
   *
   * Whether playback order is shuffled.
   *
   * A value of %FALSE indicates that playback is progressing linearly through a
   * playlist, while %TRUE means playback is progressing through a playlist in
   * some other order.
   *
   * Since: 1.0
   */
  properties [PROP_SHUFFLE] =
    g_param_spec_boolean ("shuffle", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  /**
   * ValentMediaPlayer:volume: (getter get_volume) (setter set_volume)
   *
   * The volume level.
   *
   * Attempts to change this property may be ignored by some implementations.
   *
   * Since: 1.0
   */
  properties [PROP_VOLUME] =
    g_param_spec_double ("volume", NULL, NULL,
                         0.0, G_MAXDOUBLE,
                         0.0,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);

  /**
   * ValentMediaPlayer::changed:
   * @player: a #ValentMediaPlayer
   *
   * Emitted when the state or properties of @player changes.
   *
   * This signal is a convenience for notifying of multiple changes in a single
   * emission. Implementations may emit [signal@GObject.Object::notify] for
   * properties, but must emit this signal for handlers that may be connected to
   * [signal@Valent.Media::player-changed].
   *
   * Since: 1.0
   */
  signals [CHANGED] =
    g_signal_new ("changed",
                  VALENT_TYPE_MEDIA_PLAYER,
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  /**
   * ValentMediaPlayer::seeked:
   * @player: a #ValentMediaPlayer
   * @position: the new position in microseconds
   *
   * Emitted when the track position has changed in a way that is
   * inconsistent with the current playing state.
   *
   * When this signal is not received, clients should assume that:
   *
   * - When playing, the position progresses.
   * - When paused, it remains constant.
   *
   * This signal does not need to be emitted when playback starts or when the
   * track changes, unless the track is starting at an unexpected position. An
   * expected position would be the last known one when going from Paused to
   * Playing, and `0` when going from Stopped to Playing.
   *
   * Since: 1.0
   */
  signals [SEEKED] =
    g_signal_new ("seeked",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1, G_TYPE_INT64);
}

static void
valent_media_player_init (ValentMediaPlayer *self)
{
}

/**
 * valent_media_player_emit_changed:
 * @player: a #ValentMediaPlayer
 *
 * Emit [signal@Valent.MediaPlayer::changed] on @player.
 *
 * This method should only be called by implementations of
 * [class@Valent.MediaPlayer]. Signal handlers may query the state, so it must
 * emitted after the internal representation has been updated.
 *
 * Since: 1.0
 */
void
valent_media_player_emit_changed (ValentMediaPlayer *player)
{
  g_return_if_fail (VALENT_IS_MEDIA_PLAYER (player));

  g_signal_emit (G_OBJECT (player), signals [CHANGED], 0);
}

/**
 * valent_media_player_emit_seeked:
 * @player: a #ValentMediaPlayer
 * @offset: an offset
 *
 * Emit [signal@Valent.MediaPlayer::seeked].
 *
 * This method should only be called by implementations of
 * [class@Valent.MediaPlayer]. Signal handlers may query the state, so it must
 * emitted after the internal representation has been updated.
 *
 * Since: 1.0
 */
void
valent_media_player_emit_seeked (ValentMediaPlayer *player,
                                 gint64             offset)
{
  g_return_if_fail (VALENT_IS_MEDIA_PLAYER (player));

  g_signal_emit (G_OBJECT (player), signals [SEEKED], 0, offset);
}

/**
 * valent_media_player_is_playing:
 * @player: a #ValentMediaPlayer
 *
 * A convenience for checking if @player's playback status is `Playing`.
 *
 * Returns: %TRUE if playing, %FALSE if not
 *
 * Since: 1.0
 */
gboolean
valent_media_player_is_playing (ValentMediaPlayer *player)
{
  g_return_val_if_fail (VALENT_IS_MEDIA_PLAYER (player), FALSE);

  return valent_media_player_get_state (player) == VALENT_MEDIA_STATE_PLAYING;
}

/**
 * valent_media_player_get_flags: (virtual get_flags) (get-property flags)
 * @player: a #ValentMediaPlayer
 *
 * Get flags describing the available actions of @player.
 *
 * Returns: a bitmask of #ValentMediaActions
 *
 * Since: 1.0
 */
ValentMediaActions
valent_media_player_get_flags (ValentMediaPlayer *player)
{
  ValentMediaActions ret;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_MEDIA_PLAYER (player), VALENT_MEDIA_ACTION_NONE);

  ret = VALENT_MEDIA_PLAYER_GET_CLASS (player)->get_flags (player);

  VALENT_RETURN (ret);
}

/**
 * valent_media_player_get_metadata: (virtual get_metadata) (get-property metadata)
 * @player: a #ValentMediaPlayer
 *
 * Get the metadata of the active media items.
 *
 * Implementations should typically have an entry for the `mpris:length` field.
 * Other fields generally supported by KDE Connect clients include
 * `mpris:artUrl`, `xesam:artist`, `xesam:album` and `xesam:title`.
 *
 * Returns: (transfer full): a #GVariant of type `a{sv}`
 *
 * Since: 1.0
 */
GVariant *
valent_media_player_get_metadata (ValentMediaPlayer *player)
{
  GVariant *ret;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_MEDIA_PLAYER (player), NULL);

  ret = VALENT_MEDIA_PLAYER_GET_CLASS (player)->get_metadata (player);

  if G_UNLIKELY (ret == NULL)
    ret = g_variant_parse (G_VARIANT_TYPE_VARDICT, "{}", NULL, NULL, NULL);

  VALENT_RETURN (ret);
}

/**
 * valent_media_player_get_name: (virtual get_name) (get-property name)
 * @player: a #ValentMediaPlayer
 *
 * Get the display name of the @player.
 *
 * Returns: (transfer none): player name
 *
 * Since: 1.0
 */
const char *
valent_media_player_get_name (ValentMediaPlayer *player)
{
  const char *ret;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_MEDIA_PLAYER (player), NULL);

  ret = VALENT_MEDIA_PLAYER_GET_CLASS (player)->get_name (player);

  VALENT_RETURN (ret);
}

/**
 * valent_media_player_get_position: (virtual get_position) (get-property position)
 * @player: a #ValentMediaPlayer
 *
 * Get the current position.
 *
 * Returns: the current position
 *
 * Since: 1.0
 */
gint64
valent_media_player_get_position (ValentMediaPlayer *player)
{
  gint64 ret;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_MEDIA_PLAYER (player), 0);

  ret = VALENT_MEDIA_PLAYER_GET_CLASS (player)->get_position (player);

  VALENT_RETURN (ret);
}

/**
 * valent_media_player_set_position: (virtual set_position) (set-property position)
 * @player: a #ValentMediaPlayer
 * @position: position offset
 *
 * Set the current position.
 *
 * Since: 1.0
 */
void
valent_media_player_set_position (ValentMediaPlayer *player,
                                  gint64             position)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_MEDIA_PLAYER (player));
  g_return_if_fail (position >= 0);

  VALENT_MEDIA_PLAYER_GET_CLASS (player)->set_position (player, position);

  VALENT_EXIT;
}

/**
 * valent_media_player_get_repeat: (virtual get_repeat) (get-property repeat)
 * @player: a #ValentMediaPlayer
 *
 * Get the repeat mode for @player.
 *
 * Returns: #ValentMediaRepeat
 *
 * Since: 1.0
 */
ValentMediaRepeat
valent_media_player_get_repeat (ValentMediaPlayer *player)
{
  ValentMediaRepeat ret;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_MEDIA_PLAYER (player), VALENT_MEDIA_REPEAT_NONE);

  ret = VALENT_MEDIA_PLAYER_GET_CLASS (player)->get_repeat (player);

  VALENT_RETURN (ret);
}

/**
 * valent_media_player_set_repeat: (virtual set_repeat) (set-property repeat)
 * @player: a #ValentMediaPlayer
 * @repeat: a #ValentMediaRepeat
 *
 * Set the repeat mode of @player to @repeat.
 *
 * Since: 1.0
 */
void
valent_media_player_set_repeat (ValentMediaPlayer *player,
                               ValentMediaRepeat   repeat)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_MEDIA_PLAYER (player));

  VALENT_MEDIA_PLAYER_GET_CLASS (player)->set_repeat (player, repeat);

  VALENT_EXIT;
}

/**
 * valent_media_player_get_shuffle: (virtual get_shuffle) (get-property shuffle)
 * @player: a #ValentMediaPlayer
 *
 * Get whether playback order is shuffled.
 *
 * Returns: the shuffle state
 *
 * Since: 1.0
 */
gboolean
valent_media_player_get_shuffle (ValentMediaPlayer *player)
{
  gboolean ret;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_MEDIA_PLAYER (player), FALSE);

  ret = VALENT_MEDIA_PLAYER_GET_CLASS (player)->get_shuffle (player);

  VALENT_RETURN (ret);
}

/**
 * valent_media_player_set_shuffle: (virtual set_shuffle) (set-property shuffle)
 * @player: a #ValentMediaPlayer
 * @shuffle: shuffle state
 *
 * Set whether playback order is shuffled.
 *
 * Since: 1.0
 */
void
valent_media_player_set_shuffle (ValentMediaPlayer *player,
                                  gboolean           shuffle)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_MEDIA_PLAYER (player));

  VALENT_MEDIA_PLAYER_GET_CLASS (player)->set_shuffle (player, shuffle);

  VALENT_EXIT;
}

/**
 * valent_media_player_get_state: (virtual get_state) (get-property state)
 * @player: a #ValentMediaPlayer
 *
 * Get the playback state for @player.
 *
 * Returns: #ValentMediaState
 *
 * Since: 1.0
 */
ValentMediaState
valent_media_player_get_state (ValentMediaPlayer *player)
{
  ValentMediaState ret;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_MEDIA_PLAYER (player), VALENT_MEDIA_STATE_STOPPED);

  ret = VALENT_MEDIA_PLAYER_GET_CLASS (player)->get_state (player);

  VALENT_RETURN (ret);
}

/**
 * valent_media_player_get_volume: (virtual get_volume) (get-property volume)
 * @player: a #ValentMediaPlayer
 *
 * Get the volume level.
 *
 * Returns: the volume of @player
 *
 * Since: 1.0
 */
double
valent_media_player_get_volume (ValentMediaPlayer *player)
{
  double ret;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_MEDIA_PLAYER (player), 0.0);

  ret = VALENT_MEDIA_PLAYER_GET_CLASS (player)->get_volume (player);

  VALENT_RETURN (ret);
}

/**
 * valent_media_player_set_volume: (virtual set_volume) (set-property volume)
 * @player: a #ValentMediaPlayer
 * @volume: volume level
 *
 * Set the volume level of @player.
 *
 * Since: 1.0
 */
void
valent_media_player_set_volume (ValentMediaPlayer *player,
                                double             volume)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_MEDIA_PLAYER (player));

  VALENT_MEDIA_PLAYER_GET_CLASS (player)->set_volume (player, volume);

  VALENT_EXIT;
}

/**
 * valent_media_player_next: (virtual next)
 * @player: a #ValentMediaPlayer
 *
 * Skip to the next media item.
 *
 * If there is no next track (and endless playback and track repeat are both
 * off), stop playback. If playback is paused or stopped, it remains that way.
 *
 * If [property@Valent.MediaPlayer:flags] does not include
 * %VALENT_MEDIA_ACTION_NEXT, calling this method should have no effect.
 *
 * Since: 1.0
 */
void
valent_media_player_next (ValentMediaPlayer *player)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_MEDIA_PLAYER (player));

  VALENT_MEDIA_PLAYER_GET_CLASS (player)->next (player);

  VALENT_EXIT;
}

/**
 * valent_media_player_pause: (virtual pause)
 * @player: a #ValentMediaPlayer
 *
 * Pauses playback.
 *
 * If playback is already paused, this has no effect. Calling
 * [method@Valent.MediaPlayer.pause] after this should cause playback to start
 * again from the same position.
 *
 * If [property@Valent.MediaPlayer:flags] does not include
 * %VALENT_MEDIA_ACTION_PAUSE, calling this method should have no effect.
 *
 * Since: 1.0
 */
void
valent_media_player_pause (ValentMediaPlayer *player)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_MEDIA_PLAYER (player));

  VALENT_MEDIA_PLAYER_GET_CLASS (player)->pause (player);

  VALENT_EXIT;
}

/**
 * valent_media_player_play: (virtual play)
 * @player: a #ValentMediaPlayer
 *
 * Start playback.
 *
 * If already playing, this has no effect. If paused, playback resumes from the
 * current position. If there is no track to play, this has no effect.
 *
 * If [property@Valent.MediaPlayer:flags] does not include
 * %VALENT_MEDIA_ACTION_PLAY, calling this method should have no effect.
 *
 * Since: 1.0
 */
void
valent_media_player_play (ValentMediaPlayer *player)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_MEDIA_PLAYER (player));

  VALENT_MEDIA_PLAYER_GET_CLASS (player)->play (player);

  VALENT_EXIT;
}

/**
 * valent_media_player_play_pause: (virtual play_pause)
 * @player: a #ValentMediaPlayer
 *
 * Start or pause playback, depending on the current state.
 *
 * If playback is already paused, resumes playback. If playback is stopped,
 * starts playback.
 *
 * If [property@Valent.MediaPlayer:flags] does not include
 * %VALENT_MEDIA_ACTION_PLAY or %VALENT_MEDIA_ACTION_PAUSE, calling this method
 * should have no effect.
 *
 * Since: 1.0
 */
void
valent_media_player_play_pause (ValentMediaPlayer *player)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_MEDIA_PLAYER (player));

  VALENT_MEDIA_PLAYER_GET_CLASS (player)->play_pause (player);

  VALENT_EXIT;
}

/**
 * valent_media_player_previous: (virtual previous)
 * @player: a #ValentMediaPlayer
 *
 * Skip to the previous media item.
 *
 * If there is no previous track (and endless playback and track repeat are both
 * off), stop playback. If playback is paused or stopped, it remains that way.
 *
 * If [property@Valent.MediaPlayer:flags] does not include
 * %VALENT_MEDIA_ACTION_PREVIOUS, calling this method should have no effect.
 *
 * Since: 1.0
 */
void
valent_media_player_previous (ValentMediaPlayer *player)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_MEDIA_PLAYER (player));

  VALENT_MEDIA_PLAYER_GET_CLASS (player)->previous (player);

  VALENT_EXIT;
}

/**
 * valent_media_player_seek: (virtual seek)
 * @player: a #ValentMediaPlayer
 * @offset: number of microseconds to seek forward
 *
 * Seek in the current media item by @offset microseconds.
 *
 * A negative value seeks back. If this would mean seeking back further than the
 * start of the track, the position is set to `0`. If the value passed in would
 * mean seeking beyond the end of the track, acts like a call to
 * valent_media_player_seek().
 *
 * If [property@Valent.MediaPlayer:flags] does not include
 * %VALENT_MEDIA_ACTION_SEEK, calling this method should have no effect.
 *
 * Since: 1.0
 */
void
valent_media_player_seek (ValentMediaPlayer *player,
                          gint64             offset)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_MEDIA_PLAYER (player));

  VALENT_MEDIA_PLAYER_GET_CLASS (player)->seek (player, offset);

  VALENT_EXIT;
}

/**
 * valent_media_player_stop: (virtual stop)
 * @player: a #ValentMediaPlayer
 *
 * Stop playback.
 *
 * If playback is already stopped, this has no effect. Calling
 * valent_media_player_play() after this should cause playback to start again
 * from the beginning of the track.
 *
 * If [property@Valent.MediaPlayer:flags] does not include
 * %VALENT_MEDIA_ACTION_STOP, calling this method should have no effect.
 *
 * Since: 1.0
 */
void
valent_media_player_stop (ValentMediaPlayer *player)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_MEDIA_PLAYER (player));

  VALENT_MEDIA_PLAYER_GET_CLASS (player)->stop (player);

  VALENT_EXIT;
}

