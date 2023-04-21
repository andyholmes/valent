// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-mpris-player"

#include "config.h"

#include <math.h>

#include <gio/gio.h>
#include <valent.h>

#include "valent-mpris-utils.h"


/*
 * DBus Interfaces
 */
static const char mpris_xml[] =
  "<node name='/org/mpris/MediaPlayer2'>"
  "  <interface name='org.mpris.MediaPlayer2'>"
  "    <method name='Raise'/>"
  "    <method name='Quit'/>"
  "    <property name='CanQuit' type='b' access='read'/>"
  "    <property name='Fullscreen' type='b' access='readwrite'/>"
  "    <property name='CanSetFullscreen' type='b' access='read'/>"
  "    <property name='CanRaise' type='b' access='read'/>"
  "    <property name='HasTrackList' type='b' access='read'/>"
  "    <property name='Identity' type='s' access='read'/>"
  "    <property name='DesktopEntry' type='s' access='read'/>"
  "    <property name='SupportedUriSchemes' type='as' access='read'/>"
  "    <property name='SupportedMimeTypes' type='as' access='read'/>"
  "  </interface>"
  "  <interface name='org.mpris.MediaPlayer2.Player'>"
  "    <method name='Next'/>"
  "    <method name='Previous'/>"
  "    <method name='Pause'/>"
  "    <method name='PlayPause'/>"
  "    <method name='Stop'/>"
  "    <method name='Play'/>"
  "    <method name='Seek'>"
  "      <arg direction='in' type='x' name='Offset'/>"
  "    </method>"
  "    <method name='SetPosition'>"
  "      <arg direction='in' type='o' name='TrackId'/>"
  "      <arg direction='in' type='x' name='Position'/>"
  "    </method>"
  "    <method name='OpenUri'>"
  "      <arg direction='in' type='s' name='Uri'/>"
  "    </method>"
  "    <property name='PlaybackStatus' type='s' access='read'/>"
  "    <property name='LoopStatus' type='s' access='readwrite'/>"
  "    <property name='Rate' type='d' access='readwrite'/>"
  "    <property name='Shuffle' type='b' access='readwrite'/>"
  "    <property name='Metadata' type='a{sv}' access='read'/>"
  "    <property name='Volume' type='d' access='readwrite'/>"
  "    <property name='Position' type='x' access='read'/>"
  "    <property name='MinimumRate' type='d' access='read'/>"
  "    <property name='MaximumRate' type='d' access='read'/>"
  "    <property name='CanGoNext' type='b' access='read'/>"
  "    <property name='CanGoPrevious' type='b' access='read'/>"
  "    <property name='CanPlay' type='b' access='read'/>"
  "    <property name='CanPause' type='b' access='read'/>"
  "    <property name='CanSeek' type='b' access='read'/>"
  "    <property name='CanControl' type='b' access='read'/>"
  "    <signal name='Seeked'>"
  "      <arg name='Position' type='x'/>"
  "    </signal>"
  "  </interface>"
  "</node>";


static inline GDBusNodeInfo *
valent_mpris_get_info (void)
{
  static GDBusNodeInfo *mpris_info = NULL;
  static size_t guard = 0;

  if (g_once_init_enter (&guard))
    {
      mpris_info = g_dbus_node_info_new_for_xml (mpris_xml, NULL);
      g_dbus_interface_info_cache_build (mpris_info->interfaces[0]);
      g_dbus_interface_info_cache_build (mpris_info->interfaces[1]);

      g_once_init_leave (&guard, 1);
    }

  return mpris_info;
}

/**
 * valent_mpris_get_application_iface:
 *
 * Get a #GDBusInterfaceInfo for the `org.mpris.MediaPlayer2` interface.
 *
 * Returns: (transfer none): a #GDBusInterfaceInfo
 */
GDBusInterfaceInfo *
valent_mpris_get_application_iface (void)
{
  GDBusNodeInfo *mpris_info = valent_mpris_get_info ();

  return mpris_info->interfaces[0];
}

/**
 * valent_mpris_get_player_iface:
 *
 * Get a #GDBusInterfaceInfo for the `org.mpris.MediaPlayer2.Player` interface.
 *
 * Returns: (transfer none): a #GDBusInterfaceInfo
 */
GDBusInterfaceInfo *
valent_mpris_get_player_iface (void)
{
  GDBusNodeInfo *mpris_info = valent_mpris_get_info ();

  return mpris_info->interfaces[1];
}

/**
 * valent_mpris_repeat_from_string:
 * @loop_status: repeat mode to translate
 *
 * Translate an MPRIS `LoopStatus` string to a #ValentMediaRepeat.
 *
 * Returns: (transfer none): a repeat mode
 */
ValentMediaRepeat
valent_mpris_repeat_from_string (const char *loop_status)
{
  g_return_val_if_fail (loop_status != NULL, VALENT_MEDIA_REPEAT_NONE);

  if (strcmp (loop_status, "None") == 0)
    return VALENT_MEDIA_REPEAT_NONE;

  if (strcmp (loop_status, "Playlist") == 0)
    return VALENT_MEDIA_REPEAT_ALL;

  if (strcmp (loop_status, "Track") == 0)
    return VALENT_MEDIA_REPEAT_ONE;

  return VALENT_MEDIA_REPEAT_NONE;
}

/**
 * valent_mpris_repeat_to_string:
 * @repeat: repeat mode to translate
 *
 * Translate a #ValentMediaRepeat enum to an MPRIS `LoopStatus` string.
 *
 * Returns: (transfer none): a status string
 */
const char *
valent_mpris_repeat_to_string (ValentMediaRepeat repeat)
{
  g_return_val_if_fail (repeat <= VALENT_MEDIA_REPEAT_ONE, "None");

  if (repeat == VALENT_MEDIA_REPEAT_NONE)
      return "None";

  if (repeat == VALENT_MEDIA_REPEAT_ALL)
      return "Playlist";

  if (repeat == VALENT_MEDIA_REPEAT_ONE)
      return "Track";

  return "None";
}

/**
 * valent_mpris_state_from_string:
 * @playback_status: playback mode to translate
 *
 * Translate an MPRIS `PlaybackStatus` string to a #ValentMediaState.
 *
 * Returns: (transfer none): a playback state
 */
ValentMediaState
valent_mpris_state_from_string (const char *playback_status)
{
  g_return_val_if_fail (playback_status != NULL, VALENT_MEDIA_STATE_STOPPED);

  if (strcmp (playback_status, "Stopped") == 0)
    return VALENT_MEDIA_STATE_STOPPED;

  if (strcmp (playback_status, "Playing") == 0)
    return VALENT_MEDIA_STATE_PLAYING;

  if (strcmp (playback_status, "Paused") == 0)
    return VALENT_MEDIA_STATE_PAUSED;

  return VALENT_MEDIA_STATE_STOPPED;
}

/**
 * valent_mpris_state_to_string:
 * @state: playback mode to translate
 *
 * Translate a #ValentMediaState enum to an MPRIS `PlaybackStatus` string.
 *
 * Returns: (transfer none): a status string
 */
const char *
valent_mpris_state_to_string (ValentMediaState state)
{
  g_return_val_if_fail (state <= VALENT_MEDIA_STATE_PAUSED, "Stopped");

  if (state == VALENT_MEDIA_STATE_STOPPED)
      return "Stopped";

  if (state == VALENT_MEDIA_STATE_PLAYING)
      return "Playing";

  if (state == VALENT_MEDIA_STATE_PAUSED)
      return "Paused";

  return "Stopped";
}

/**
 * valent_mpris_get_time:
 *
 * Get a monotonic timestamp, in seconds.
 *
 * Returns: a timestamp in seconds
 */
double
valent_mpris_get_time (void)
{
  return floor (g_get_real_time () / G_TIME_SPAN_SECOND);
}

