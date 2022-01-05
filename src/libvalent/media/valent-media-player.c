// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-media-player"

#include "config.h"

#include <gio/gio.h>

#include "valent-media-enums.h"
#include "valent-media-player.h"


/**
 * SECTION:valentmediaplayer
 * @short_description: Base class for media players
 * @title: ValentMediaPlayer
 * @stability: Unstable
 * @include: libvalent-media.h
 *
 * A #ValentMediaPlayer is a base class for media players that more or less
 * mirrors the MPRISv2 specification. The primary difference is that media
 * player control is not spread across several interfaces as is the case with
 * the application, player, playlist and tracklist DBus interfaces.
 *
 * The built-in MPRIS plugin provides an implementation for MPRISv2 players, so
 * it is primarily an abstraction for the sake of plugins that want to control
 * the media state in response to certain events.
 */

G_DEFINE_TYPE (ValentMediaPlayer, valent_media_player, G_TYPE_OBJECT)

/**
 * ValentMediaPlayerClass:
 * @parent_class: The parent class.
 * @changed: the class closure for #ValentMediaPlayer::changed signal
 * @next: the virtual function pointer for valent_media_player_next()
 * @open_uri: the virtual function pointer for valent_media_player_open_uri()
 * @pause: the virtual function pointer for valent_media_player_pause()
 * @play: the virtual function pointer for valent_media_player_play()
 * @play_pause: the virtual function pointer for valent_media_player_play_pause()
 * @previous: the virtual function pointer for valent_media_player_previous()
 * @seek: the virtual function pointer for valent_media_player_seek()
 * @set_position: the virtual function pointer for valent_media_player_set_position()
 * @stop: the virtual function pointer for valent_media_player_stop()
 * @get_flags: Getter for the #ValentMediaPlayer:flags property.
 * @get_metadata: Getter for the #ValentMediaPlayer:metadata property.
 * @get_name: Getter for the #ValentMediaPlayer:name property.
 * @get_position: Getter for the #ValentMediaPlayer:position property.
 * @seeked: the class closure for the #ValentMediaPlayer::seeked signal
 * @get_state: Getter for the #ValentMediaPlayer:state property.
 * @set_state: Setter for the #ValentMediaPlayer:state property.
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

static ValentMediaState
valent_media_player_real_get_state (ValentMediaPlayer *player)
{
  return VALENT_MEDIA_STATE_STOPPED;
}

static void
valent_media_player_real_set_state (ValentMediaPlayer *player,
                                    ValentMediaState   state)
{
  g_debug ("%s: Operation not supported (%u)", G_STRFUNC, state);
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
  g_debug ("%s: Operation not supported", G_STRFUNC);
}

static void
valent_media_player_real_next (ValentMediaPlayer *player)
{
  g_debug ("%s: Operation not supported", G_STRFUNC);
}

static void
valent_media_player_real_pause (ValentMediaPlayer *player)
{
  g_debug ("%s: Operation not supported", G_STRFUNC);
}

static void
valent_media_player_real_play (ValentMediaPlayer *player)
{
  g_debug ("%s: Operation not supported", G_STRFUNC);
}

static void
valent_media_player_real_play_pause (ValentMediaPlayer *player)
{
  ValentMediaActions flags = valent_media_player_get_flags (player);
  ValentMediaState state = valent_media_player_get_state (player);

  if ((state & VALENT_MEDIA_STATE_STOPPED) == 0 &&
      (flags & VALENT_MEDIA_ACTION_PAUSE) != 0)
    valent_media_player_pause (player);

  else if ((state & VALENT_MEDIA_STATE_STOPPED) != 0 &&
           (flags & VALENT_MEDIA_ACTION_PLAY) != 0)
    valent_media_player_play (player);
}

static void
valent_media_player_real_previous (ValentMediaPlayer *player)
{
  g_debug ("%s: Operation not supported", G_STRFUNC);
}

static void
valent_media_player_real_seek (ValentMediaPlayer *player,
                               gint64             offset)
{
  g_debug ("%s: Operation not supported", G_STRFUNC);
}

static void
valent_media_player_real_stop (ValentMediaPlayer *player)
{
  g_debug ("%s: Operation not supported", G_STRFUNC);
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

    case PROP_STATE:
      g_value_set_flags (value, valent_media_player_get_state (self));
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
    case PROP_STATE:
      valent_media_player_set_state (self, g_value_get_flags (value));
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
  player_class->get_state = valent_media_player_real_get_state;
  player_class->set_state = valent_media_player_real_set_state;
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
   * A bitmask of #ValentMediaActions that are actionable on the player.
   */
  properties [PROP_FLAGS] =
    g_param_spec_flags ("flags",
                        "Flags",
                        "Action Flags",
                        VALENT_TYPE_MEDIA_ACTIONS,
                        VALENT_MEDIA_ACTION_NONE,
                        (G_PARAM_READABLE |
                         G_PARAM_STATIC_STRINGS));

  /**
   * ValentMediaPlayer:metadata:
   *
   * The metadata of the current element.
   *
   * If there is a current track, this must have a "mpris:trackid" entry (of
   * D-Bus type "o") at the very least, which contains a D-Bus path that
   * uniquely identifies this track.
   *
   * See the type documentation for more details.
   */
  properties [PROP_METADATA] =
    g_param_spec_variant ("metadata",
                          "Metadata",
                          "Metadata",
                          G_VARIANT_TYPE ("a{sv}"),
                          NULL,
                          (G_PARAM_READABLE |
                           G_PARAM_STATIC_STRINGS));

  /**
   * ValentMediaPlayer:name:
   *
   * The name of the media player, corresponding to the `Identity` property of
   * the `org.mpris.MediaPlayer2` interface.
   */
  properties [PROP_NAME] =
    g_param_spec_string ("name",
                         "Name",
                         "Name",
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentMediaPlayer:position:
   *
   * The current track position in microseconds, between `0` and the
   * 'mpris:length' metadata entry (see #ValentMediaPlayer:position).
   *
   * Note: If the media player allows it, the current playback position can be
   * changed either the SetPosition method or the Seek method on this interface.
   * If this is not the case, the CanSeek property is %FALSE, and setting this
   * property has no effect.
   *
   * If the playback progresses in a way that is inconstistant with the Rate
   * property, the Seeked signal is emitted.
   */
  properties [PROP_POSITION] =
    g_param_spec_int64 ("position",
                        "Position",
                        "Position",
                        G_MININT64, G_MAXINT64,
                        0,
                        (G_PARAM_READABLE |
                         G_PARAM_STATIC_STRINGS));

  /**
   * ValentMediaPlayer:state:
   *
   * A bitmask of #ValentMediaState that describes the playback state.
   */
  properties [PROP_STATE] =
    g_param_spec_flags ("state",
                        "State",
                        "State Flags",
                        VALENT_TYPE_MEDIA_STATE,
                        VALENT_MEDIA_STATE_STOPPED,
                        (G_PARAM_READWRITE |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  /**
   * ValentMediaPlayer:volume:
   *
   * The volume level.
   *
   * When setting, if a negative value is passed, the volume should be set to
   * `0.0`.
   *
   * Implementations subclassing #ValentMediaPlayer may ignore attempts to
   * change this property.
   */
  properties [PROP_VOLUME] =
    g_param_spec_double ("volume",
                         "Volume",
                         "Volume",
                         0.0, G_MAXDOUBLE,
                         0.0,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);

  /**
   * ValentMediaPlayer::changed:
   * @player: a #ValentMediaPlayer
   * @position: the new position in microseconds
   *
   * #ValentMediaPlayer::changed is a convenience function for notifying of
   * multiple property changes. Implementations may emit GObject::notify but
   * must emit #ValentMediaPlayer::changed.
   */
  signals [CHANGED] =
    g_signal_new ("changed",
                  VALENT_TYPE_MEDIA_PLAYER,
                  G_SIGNAL_RUN_FIRST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  /**
   * ValentMediaPlayer::seeked:
   * @player: a #ValentMediaPlayer
   * @position: the new position in microseconds
   *
   * #ValentMediaPlayer::seeked is emitted when the track position has changed
   * in a way that is inconsistent with the current playing state.
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
 * Emit ValentMediaPlayer::changed.
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
 * Emit ValentMediaPlayer::seeked.
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
 * Returns: %TRUE if playing
 */
gboolean
valent_media_player_is_playing (ValentMediaPlayer *player)
{
  ValentMediaState state;

  g_return_val_if_fail (VALENT_IS_MEDIA_PLAYER (player), FALSE);

  state = valent_media_player_get_state (player);

  if ((state & VALENT_MEDIA_STATE_PAUSED) != 0)
    return FALSE;

  if ((state & VALENT_MEDIA_STATE_PLAYING) != 0)
    return TRUE;

  return FALSE;
}

/**
 * valent_media_player_get_flags:
 * @player: a #ValentMediaPlayer
 *
 * Get the available actions for @player.
 *
 * Returns: #ValentMediaActions
 */
ValentMediaActions
valent_media_player_get_flags (ValentMediaPlayer *player)
{
  g_return_val_if_fail (VALENT_IS_MEDIA_PLAYER (player), VALENT_MEDIA_ACTION_NONE);

  return VALENT_MEDIA_PLAYER_GET_CLASS (player)->get_flags (player);
}

/**
 * valent_media_player_get_metadata:
 * @player: a #ValentMediaPlayer
 *
 * Get the current track's metadata. See #ValentMediaPlayer:metadata.
 *
 * Returns: (transfer full): a #GVariant
 */
GVariant *
valent_media_player_get_metadata (ValentMediaPlayer *player)
{
  g_return_val_if_fail (VALENT_IS_MEDIA_PLAYER (player), NULL);

  return VALENT_MEDIA_PLAYER_GET_CLASS (player)->get_metadata (player);
}

/**
 * valent_media_player_get_name:
 * @player: a #ValentMediaPlayer
 *
 * Get the display name of the media player. This corresponds to the `Identity`
 * property of the org.mpris.MediaPlayer2 interface.
 *
 * Returns: (transfer none): player name
 */
const char *
valent_media_player_get_name (ValentMediaPlayer *player)
{
  g_return_val_if_fail (VALENT_IS_MEDIA_PLAYER (player), NULL);

  return VALENT_MEDIA_PLAYER_GET_CLASS (player)->get_name (player);
}

/**
 * valent_media_player_get_position:
 * @player: a #ValentMediaPlayer
 *
 * Get the current position. See #ValentMediaPlayer:position.
 *
 * Returns: the current position
 */
gint64
valent_media_player_get_position (ValentMediaPlayer *player)
{
  g_return_val_if_fail (VALENT_IS_MEDIA_PLAYER (player), 0);

  return VALENT_MEDIA_PLAYER_GET_CLASS (player)->get_position (player);
}

/**
 * valent_media_player_set_position:
 * @player: a #ValentMediaPlayer
 * @track_id: track ID
 * @position: position offset
 *
 * Set the current position. See #ValentMediaPlayer:position.
 */
void
valent_media_player_set_position (ValentMediaPlayer *player,
                                  const char        *track_id,
                                  gint64             position)
{
  ValentMediaPlayerClass *klass;

  g_return_if_fail (VALENT_IS_MEDIA_PLAYER (player));
  g_return_if_fail (track_id != NULL);

  klass = VALENT_MEDIA_PLAYER_GET_CLASS (player);
  g_return_if_fail (klass->set_position);

  klass->set_position (player, track_id, position);
}

/**
 * valent_media_player_get_state:
 * @player: a #ValentMediaPlayer
 *
 * Get the playback state for @player.
 *
 * Returns: #ValentMediaState
 */
ValentMediaState
valent_media_player_get_state (ValentMediaPlayer *player)
{
  g_return_val_if_fail (VALENT_IS_MEDIA_PLAYER (player), VALENT_MEDIA_STATE_STOPPED);

  return VALENT_MEDIA_PLAYER_GET_CLASS (player)->get_state (player);
}

/**
 * valent_media_player_set_state:
 * @player: a #ValentMediaPlayer
 * @state: a #ValentMediaState
 *
 * Set the playback state of @player to @state.
 */
void
valent_media_player_set_state (ValentMediaPlayer *player,
                               ValentMediaState   state)
{
  g_return_if_fail (VALENT_IS_MEDIA_PLAYER (player));

  VALENT_MEDIA_PLAYER_GET_CLASS (player)->set_state (player, state);
}

/**
 * valent_media_player_get_volume:
 * @player: a #ValentMediaPlayer
 *
 * Set the volume level. See #ValentMediaPlayer:volume.
 *
 * Returns: the volume of @player
 */
double
valent_media_player_get_volume (ValentMediaPlayer *player)
{
  g_return_val_if_fail (VALENT_IS_MEDIA_PLAYER (player), 0.0);

  return VALENT_MEDIA_PLAYER_GET_CLASS (player)->get_volume (player);
}

/**
 * valent_media_player_set_volume:
 * @player: a #ValentMediaPlayer
 * @volume: volume level
 *
 * Set the volume level. See #ValentMediaPlayer:volume.
 */
void
valent_media_player_set_volume (ValentMediaPlayer *player,
                                double             volume)
{
  ValentMediaPlayerClass *klass;

  g_return_if_fail (VALENT_IS_MEDIA_PLAYER (player));

  klass = VALENT_MEDIA_PLAYER_GET_CLASS (player);
  g_return_if_fail (klass->set_volume);

  klass->set_volume (player, volume);
}

/**
 * valent_media_player_next:
 * @player: a #ValentMediaPlayer
 *
 * Skips to the next track in the tracklist.
 *
 * If there is no next track (and endless playback and track repeat are both
 * off), stop playback. If playback is paused or stopped, it remains that way.
 *
 * If #ValentMediaPlayer:flags does not include %VALENT_MEDIA_ACTION_NEXT,
 * attempting to call this method should have no effect.
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
 * valent_media_player_open_uri:
 * @player: a #ValentMediaPlayer
 * @uri: a URI
 *
 * Opens the @uri given as an argument.
 *
 * If the playback is stopped, starts playing. If the uri scheme or the
 * mime-type of the uri to open is not supported, this method does nothing. In
 * particular, if the list of available uri schemes is empty, this method may
 * not be implemented.
 *
 * Clients should not assume that @uri has been opened as soon as this method
 * returns. They should wait until the mpris:trackid field in
 * #ValentMediaPlayer:metadata property changes.
 *
 * If the media player implements the TrackList interface, then the opened track
 * should be made part of the tracklist, the
 * org.mpris.MediaPlayer2.TrackList.TrackAdded or
 * org.mpris.MediaPlayer2.TrackList.TrackListReplaced signal should be fired, as
 * well as the org.freedesktop.DBus.Properties.PropertiesChanged signal on the
 * tracklist interface.
 */
void
valent_media_player_open_uri (ValentMediaPlayer *player,
                              const char        *uri)
{
  ValentMediaPlayerClass *klass;

  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_MEDIA_PLAYER (player));
  g_return_if_fail (uri != NULL);

  klass = VALENT_MEDIA_PLAYER_GET_CLASS (player);
  g_return_if_fail (klass->open_uri);

  klass->open_uri (player, uri);

  VALENT_EXIT;
}

/**
 * valent_media_player_pause:
 * @player: a #ValentMediaPlayer
 *
 * Pauses playback.
 *
 * If playback is already paused, this has no effect. Calling
 * valent_media_player_play() after this should cause playback to start again
 * from the same position.
 *
 * If #ValentMediaPlayer:flags does not include %VALENT_MEDIA_ACTION_PAUSE,
 * attempting to call this method should have no effect.
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
 * valent_media_player_play:
 * @player: a #ValentMediaPlayer
 *
 * Starts or resumes playback.
 *
 * If already playing, this has no effect. If paused, playback resumes from the
 * current position. If there is no track to play, this has no effect.
 *
 * If #ValentMediaPlayer:flags does not include %VALENT_MEDIA_ACTION_PLAY,
 * attempting to call this method should have no effect.
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
 * valent_media_player_play_pause:
 * @player: a #ValentMediaPlayer
 *
 * Pauses playback.
 *
 * If playback is already paused, resumes playback. If playback is stopped,
 * starts playback.
 *
 * If #ValentMediaPlayer:flags does not include %VALENT_MEDIA_ACTION_PLAY or
 * %VALENT_MEDIA_ACTION_PAUSE, attempting to call this method should have no
 * effect.
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
 * valent_media_player_previous:
 * @player: a #ValentMediaPlayer
 *
 * Skips to the previous track in the tracklist.
 *
 * If there is no previous track (and endless playback and track repeat are both
 * off), stop playback. If playback is paused or stopped, it remains that way.
 *
 * If #ValentMediaPlayer:flags does not include %VALENT_MEDIA_ACTION_PREVIOUS,
 * attempting to call this method should have no effect.
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
 * valent_media_player_seek:
 * @player: a #ValentMediaPlayer
 * @offset: number of microseconds to seek forward
 *
 * Seeks forward in the current track by the specified number of microseconds.
 *
 * A negative value seeks back. If this would mean seeking back further than the
 * start of the track, the position is set to `0`. If the value passed in would
 * mean seeking beyond the end of the track, acts like a call to
 * valent_media_player_seek().
 *
 * If #ValentMediaPlayer:flags does not include %VALENT_MEDIA_ACTION_SEEK,
 * attempting to call this method should have no effect.
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
 * valent_media_player_stop:
 * @player: a #ValentMediaPlayer
 *
 * Stops playback.
 *
 * If playback is already stopped, this has no effect. Calling
 * valent_media_player_play() after this should cause playback to start again
 * from the beginning of the track.
 *
 * If #ValentMediaPlayer:flags does not include %VALENT_MEDIA_ACTION_STOP,
 * attempting to call this method should have no effect.
 */
void
valent_media_player_stop (ValentMediaPlayer *player)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_MEDIA_PLAYER (player));

  VALENT_MEDIA_PLAYER_GET_CLASS (player)->stop (player);

  VALENT_EXIT;
}

